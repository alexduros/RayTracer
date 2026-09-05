// ---------------------------------------------------------------------------
// raymini GUI
//
// Left: real-time OpenGL 3.3 preview of the mesh (orbit with the mouse).
// Right: the raytracer's output for the same camera.
// Bottom: controls. The raytracer itself lives in src/core and is also driven
// headlessly by raymini-cli, so nothing in here is needed to render.
// ---------------------------------------------------------------------------

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "Image.h"
#include "Mesh.h"
#include "RayTracer.h"
#include "Scene.h"

namespace {

constexpr int kWindowWidth = 1400;
constexpr int kWindowHeight = 800;
constexpr int kViewportWidth = 600;
constexpr int kViewportHeight = 400;

struct GLVertex {
    float x, y, z;     // position
    float nx, ny, nz;  // smooth normal from Mesh::loadOFF
    float r, g, b;     // vertex colour (Y gradient)
};

// ---- orbit camera state (shared with the GLFW callbacks) -------------------
glm::vec3 cameraPos(0.f, 0.f, 5.f);
glm::vec3 cameraFront(0.f, 0.f, -1.f);
glm::vec3 cameraUp(0.f, 1.f, 0.f);
glm::vec3 cameraTarget(0.f, 0.f, 0.f);
float initialDistance = 5.f;
float yaw = -90.f;
float pitch = 0.f;
float lastX = 0.f, lastY = 0.f;
bool firstMouse = true;
bool leftMouseButtonPressed = false;
bool viewerHovered = false;   // refreshed every frame; input outside the viewer is ignored
float fov = 45.f;

void resetCamera() {
    yaw = -90.f;
    pitch = 0.f;
    fov = 45.f;
    cameraFront = glm::vec3(0.f, 0.f, -1.f);
    cameraPos = cameraTarget + glm::vec3(0.f, 0.f, initialDistance);
}

// Look down -Z at the bbox centre from twice the largest extent.
void frameModel(const BoundingBox& bbox) {
    const Vec3Df c = bbox.getCenter();
    cameraTarget = glm::vec3(c[0], c[1], c[2]);
    initialDistance = std::max(bbox.getSize(), 1e-3f) * 2.f;
    resetCamera();
}

void mouseButtonCallback(GLFWwindow*, int button, int action, int) {
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;
    if (action == GLFW_PRESS && viewerHovered) {
        leftMouseButtonPressed = true;
        firstMouse = true;
    } else if (action == GLFW_RELEASE) {
        leftMouseButtonPressed = false;
        firstMouse = true;
    }
}

void cursorPosCallback(GLFWwindow*, double xpos, double ypos) {
    if (!leftMouseButtonPressed) return;
    if (firstMouse) {
        lastX = static_cast<float>(xpos);
        lastY = static_cast<float>(ypos);
        firstMouse = false;
    }
    const float sensitivity = 0.1f;
    const float xoffset = (static_cast<float>(xpos) - lastX) * sensitivity;
    const float yoffset = (lastY - static_cast<float>(ypos)) * sensitivity;  // y grows downwards
    lastX = static_cast<float>(xpos);
    lastY = static_cast<float>(ypos);

    yaw += xoffset;
    pitch = std::clamp(pitch + yoffset, -89.f, 89.f);

    glm::vec3 front;
    front.x = std::cos(glm::radians(yaw)) * std::cos(glm::radians(pitch));
    front.y = std::sin(glm::radians(pitch));
    front.z = std::sin(glm::radians(yaw)) * std::cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
    cameraPos = cameraTarget - cameraFront * glm::length(cameraPos - cameraTarget);
}

void scrollCallback(GLFWwindow*, double, double yoffset) {
    if (!viewerHovered) return;
    fov = std::clamp(fov - static_cast<float>(yoffset), 10.f, 120.f);
}

// ---- GL resources ----------------------------------------------------------
GLuint createModel(const Mesh& mesh, size_t& numIndices) {
    const std::vector<Vertex>& V = mesh.getVertices();
    const std::vector<Triangle>& T = mesh.getTriangles();

    float minY = V.empty() ? 0.f : V[0].getPos()[1];
    float maxY = minY;
    for (const Vertex& v : V) {
        minY = std::min(minY, v.getPos()[1]);
        maxY = std::max(maxY, v.getPos()[1]);
    }

    std::vector<GLVertex> vertices;
    vertices.reserve(V.size());
    for (const Vertex& v : V) {
        const Vec3Df& p = v.getPos();
        const Vec3Df& n = v.getNormal();
        const float t = (maxY > minY) ? (p[1] - minY) / (maxY - minY) : 0.5f;
        vertices.push_back({p[0], p[1], p[2],
                            n[0], n[1], n[2],
                            0.8f + 0.2f * t, 0.6f + 0.4f * (1.f - t), 0.4f + 0.6f * t});
    }

    std::vector<unsigned int> indices;
    indices.reserve(T.size() * 3);
    for (const Triangle& t : T) {
        indices.push_back(t.getVertex(0));
        indices.push_back(t.getVertex(1));
        indices.push_back(t.getVertex(2));
    }
    numIndices = indices.size();

    GLuint vao, vbo, ebo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GLVertex), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GLVertex), reinterpret_cast<void*>(offsetof(GLVertex, x)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GLVertex), reinterpret_cast<void*>(offsetof(GLVertex, nx)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(GLVertex), reinterpret_cast<void*>(offsetof(GLVertex, r)));
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    return vao;
}

const char* kVertexShader = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aColor;
out vec3 vNormal;
out vec3 vColor;
out vec3 vWorldPos;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main() {
    vec4 world = model * vec4(aPos, 1.0);
    vWorldPos = world.xyz;
    vNormal = mat3(model) * aNormal;
    vColor = aColor;
    gl_Position = projection * view * world;
}
)";

// Headlight: Lambert term from a light sitting at the camera.
const char* kFragmentShader = R"(
#version 330 core
in vec3 vNormal;
in vec3 vColor;
in vec3 vWorldPos;
uniform vec3 cameraPos;
out vec4 FragColor;
void main() {
    vec3 n = normalize(vNormal);
    vec3 l = normalize(cameraPos - vWorldPos);
    float diffuse = max(dot(n, l), 0.0);
    FragColor = vec4(vColor * (0.25 + 0.75 * diffuse), 1.0);
}
)";

GLuint compileShader(GLenum type, const char* source, const char* label) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLchar log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << "shader compile error (" << label << "):\n" << log << std::endl;
    }
    return shader;
}

GLuint createShaderProgram() {
    GLuint vs = compileShader(GL_VERTEX_SHADER, kVertexShader, "vertex");
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, kFragmentShader, "fragment");
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLchar log[1024];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::cerr << "shader link error:\n" << log << std::endl;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

// Upload an RGB image into `texture` (created on first use).
void uploadTexture(GLuint& texture, int& texW, int& texH, const Image& img) {
    if (texture == 0) glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (texW != img.width() || texH != img.height()) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, img.width(), img.height(), 0, GL_RGB, GL_UNSIGNED_BYTE, img.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        texW = img.width();
        texH = img.height();
    } else {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, img.width(), img.height(), GL_RGB, GL_UNSIGNED_BYTE, img.data());
    }
    glBindTexture(GL_TEXTURE_2D, 0);
}

const char* kModeLabels[] = {"Lit (ambient stub)", "Ambient", "Hit mask", "Normals", "Depth", "Object id"};
const char* kModeSlugs[] = {"lit", "ambient", "hitmask", "normals", "depth", "objectid"};

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <model.off>" << std::endl;
        return 1;
    }
    const std::string modelPath = argv[1];

    Scene scene;
    try {
        scene.addObjectFromOFF(modelPath);
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    scene.addDefaultLights();
    const Mesh& mesh = scene.getObjects()[0].getMesh();
    std::cout << "Loaded " << modelPath << ": " << mesh.getVertices().size() << " vertices, "
              << mesh.getTriangles().size() << " triangles" << std::endl;

    if (!glfwInit()) {
        std::cerr << "Failed to initialise GLFW" << std::endl;
        return 1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);  // required on macOS

    GLFWwindow* window = glfwCreateWindow(kWindowWidth, kWindowHeight, "Raymini", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    // Our callbacks are installed first; ImGui's backend chains to them.
    glfwSetCursorPosCallback(window, cursorPosCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);

    if (!gladLoadGL(reinterpret_cast<GLADloadfunc>(glfwGetProcAddress))) {
        std::cerr << "Failed to initialise GLAD" << std::endl;
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    size_t numIndices = 0;
    const GLuint vao = createModel(mesh, numIndices);
    frameModel(scene.getBoundingBox());

    const GLuint shaderProgram = createShaderProgram();
    const GLint modelLoc = glGetUniformLocation(shaderProgram, "model");
    const GLint viewLoc = glGetUniformLocation(shaderProgram, "view");
    const GLint projectionLoc = glGetUniformLocation(shaderProgram, "projection");
    const GLint cameraPosLoc = glGetUniformLocation(shaderProgram, "cameraPos");

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // Offscreen framebuffer the GL preview is rendered into, then shown via ImGui::Image.
    GLuint viewportFBO, viewportTexture, depthBuffer;
    glGenFramebuffers(1, &viewportFBO);
    glGenTextures(1, &viewportTexture);
    glBindTexture(GL_TEXTURE_2D, viewportTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, kViewportWidth, kViewportHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, viewportFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, viewportTexture, 0);
    glGenRenderbuffers(1, &depthBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, kViewportWidth, kViewportHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Raytracer state.
    RayTracer rt;
    Image lastRender;
    GLuint rtTexture = 0;
    int rtTexW = 0, rtTexH = 0;
    int rtResolution = 256;
    int rtMode = static_cast<int>(RayTracer::DebugMode::AMBIENT);
    float rtDepthRange = initialDistance + scene.getBoundingBox().getSize();
    std::string lastSavedPath;
    bool wireframe = false;

    const ImGuiWindowFlags panelFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ---- GL preview -----------------------------------------------------
        glBindFramebuffer(GL_FRAMEBUFFER, viewportFBO);
        glViewport(0, 0, kViewportWidth, kViewportHeight);
        glClearColor(0.f, 0.f, 0.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
        glUseProgram(shaderProgram);

        cameraFront = glm::normalize(cameraTarget - cameraPos);
        const float viewportAspect = static_cast<float>(kViewportWidth) / kViewportHeight;
        const glm::mat4 projection = glm::perspective(glm::radians(fov), viewportAspect, 0.01f * initialDistance, 100.f * initialDistance);
        const glm::mat4 view = glm::lookAt(cameraPos, cameraTarget, cameraUp);
        const glm::mat4 model(1.f);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(cameraPosLoc, 1, glm::value_ptr(cameraPos));

        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(numIndices), GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        ImGui::SetNextWindowPos(ImVec2(10, 20), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(620, 460), ImGuiCond_Always);
        ImGui::Begin("OpenGL Viewer", nullptr, panelFlags);
        ImGui::Image(static_cast<ImTextureID>(viewportTexture), ImVec2(kViewportWidth, kViewportHeight));
        viewerHovered = ImGui::IsItemHovered();
        ImGui::End();

        // ---- raytracer panel -------------------------------------------------
        ImGui::SetNextWindowPos(ImVec2(650, 20), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(730, 460), ImGuiCond_Always);
        ImGui::Begin("Raytracer", nullptr, panelFlags);

        ImGui::SliderInt("Resolution", &rtResolution, 64, 1024);
        ImGui::Combo("Mode", &rtMode, kModeLabels, IM_ARRAYSIZE(kModeLabels));
        if (static_cast<RayTracer::DebugMode>(rtMode) == RayTracer::DebugMode::DEPTH) {
            ImGui::SliderFloat("Depth range", &rtDepthRange, 0.1f, 10.f * initialDistance);
        }

        if (ImGui::Button("Render Scene")) {
            const glm::vec3 dir = glm::normalize(cameraFront);
            const glm::vec3 right = glm::normalize(glm::cross(dir, cameraUp));
            const glm::vec3 up = glm::normalize(glm::cross(right, dir));
            auto toVec3Df = [](const glm::vec3& v) { return Vec3Df(v.x, v.y, v.z); };

            rt.setDebugMode(static_cast<RayTracer::DebugMode>(rtMode));
            rt.setDepthRange(rtDepthRange);
            lastRender = rt.render(scene, toVec3Df(cameraPos), toVec3Df(dir), toVec3Df(up), toVec3Df(right),
                                   glm::radians(fov), 1.f, rtResolution, rtResolution);
            uploadTexture(rtTexture, rtTexW, rtTexH, lastRender);
            lastSavedPath.clear();
        }

        if (lastRender.isValid()) {
            ImGui::SameLine();
            if (ImGui::Button("Save PNG")) {
                std::filesystem::create_directories("renders");
                char name[128];
                std::snprintf(name, sizeof(name), "renders/render_%s_%dx%d.png", kModeSlugs[rtMode], rtTexW, rtTexH);
                lastSavedPath = lastRender.save(name) ? name : "save failed";
            }
            const RayTracer::Stats& st = rt.getLastStats();
            ImGui::SameLine();
            ImGui::Text("%dx%d in %.2fs, %.0f%% hits", rtTexW, rtTexH, st.seconds,
                        st.rays ? 100.0 * st.hits / st.rays : 0.0);
            if (!lastSavedPath.empty()) {
                ImGui::SameLine();
                ImGui::TextDisabled("%s", lastSavedPath.c_str());
            }
            ImGui::Image(static_cast<ImTextureID>(rtTexture), ImVec2(400, 400));
        } else {
            ImGui::SameLine();
            ImGui::TextDisabled("(no render yet)");
        }
        ImGui::End();

        // ---- controls ---------------------------------------------------------
        ImGui::SetNextWindowPos(ImVec2(10, 500), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(1370, 280), ImGuiCond_Always);
        ImGui::Begin("Controls", nullptr, panelFlags);
        ImGui::Columns(3, "ControlColumns", true);

        ImGui::Text("Camera");
        ImGui::Separator();
        ImGui::SliderFloat("FOV", &fov, 10.0f, 120.0f);
        ImGui::Text("Position: %.2f, %.2f, %.2f", cameraPos.x, cameraPos.y, cameraPos.z);
        ImGui::Text("Target:   %.2f, %.2f, %.2f", cameraTarget.x, cameraTarget.y, cameraTarget.z);
        ImGui::TextDisabled("Left-drag in the viewer to orbit, scroll to zoom.");
        if (ImGui::Button("Reset Camera")) resetCamera();
        ImGui::NextColumn();

        ImGui::Text("Preview");
        ImGui::Separator();
        ImGui::Checkbox("Wireframe", &wireframe);
        ImGui::NextColumn();

        ImGui::Text("Model");
        ImGui::Separator();
        ImGui::TextWrapped("%s", modelPath.c_str());
        ImGui::Text("Vertices:  %zu", mesh.getVertices().size());
        ImGui::Text("Triangles: %zu", mesh.getTriangles().size());
        ImGui::Text("Size:      %.3f", scene.getBoundingBox().getSize());
        ImGui::Columns(1);
        ImGui::End();

        // ---- present ------------------------------------------------------------
        ImGui::Render();
        int displayW, displayH;
        glfwGetFramebufferSize(window, &displayW, &displayH);
        glViewport(0, 0, displayW, displayH);
        glClearColor(0.f, 0.f, 0.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
