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
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "Camera.h"
#include "Image.h"
#include "Mesh.h"
#include "RayTracer.h"
#include "RenderJob.h"
#include "Scene.h"

// Absolute path of the bundled models/ directory, baked in by CMake.
#ifndef RAYMINI_MODELS_DIR
#define RAYMINI_MODELS_DIR ""
#endif

namespace {

constexpr int kWindowWidth = 1400;  // initial size; the layout follows resizes and full screen
constexpr int kWindowHeight = 800;
constexpr int kMinWindowWidth = 900;
constexpr int kMinWindowHeight = 600;
// The GL preview and the raytraced render share this aspect so both frame the same view.
constexpr float kViewportAspect = 3.f / 2.f;
// Layout: two panels on top (preview | render), the Controls strip below.
constexpr float kMargin = 10.f;
constexpr float kControlsHeight = 280.f;
constexpr float kMinTopHeight = 200.f;
constexpr float kPreviewShare = 0.46f;  // share of the top row's width given to the preview

// Largest w x h rectangle with the given aspect that fits in `avail`.
ImVec2 fitAspect(const ImVec2& avail, float aspect) {
    float w = std::max(1.f, avail.x);
    float h = w / aspect;
    if (h > avail.y) {
        h = std::max(1.f, avail.y);
        w = h * aspect;
    }
    return ImVec2(std::floor(w), std::floor(h));
}
// Tile edge for the background render; each finished tile is shown as it lands.
constexpr unsigned int kRenderTileSize = 32;

// Render height that keeps square pixels at the preview's aspect.
int renderHeightFor(int width) {
    return std::max(1, static_cast<int>(width / kViewportAspect + 0.5f));
}

struct GLVertex {
    float x, y, z;     // position
    float nx, ny, nz;  // smooth normal from Mesh::loadOFF
    float r, g, b;     // material colour of the owning object
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

// ---- model discovery --------------------------------------------------------
// Every *.off file in `dir`, sorted by name (empty if `dir` is not a directory).
std::vector<std::filesystem::path> listModels(const std::filesystem::path& dir) {
    std::vector<std::filesystem::path> out;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
        if (ext == ".off" || ext == ".obj") out.push_back(entry.path());
    }
    std::sort(out.begin(), out.end());
    return out;
}

// Where the picker looks: next to a model given on the command line, else the
// bundled models/ directory known at build time, else ./models.
std::filesystem::path modelsDirectory(const char* givenModel) {
    std::vector<std::filesystem::path> candidates;
    if (givenModel) candidates.push_back(std::filesystem::path(givenModel).parent_path());
    candidates.push_back(RAYMINI_MODELS_DIR);
    candidates.push_back("models");
    for (const auto& dir : candidates) {
        std::error_code ec;
        if (!dir.empty() && std::filesystem::is_directory(dir, ec)) return dir;
    }
    return {};
}

// ---- GL resources ----------------------------------------------------------
struct GLModel {
    GLuint vao = 0, vbo = 0, ebo = 0;
    size_t numIndices = 0;
};

void destroyModel(GLModel& m) {
    if (m.vao) glDeleteVertexArrays(1, &m.vao);
    if (m.vbo) glDeleteBuffers(1, &m.vbo);
    if (m.ebo) glDeleteBuffers(1, &m.ebo);
    m = GLModel();
}

// One VAO for the whole scene: every object's vertices carry its material
// colour, so the preview shows the same materials the raytracer uses.
GLModel createModel(const Scene& scene) {
    std::vector<GLVertex> vertices;
    std::vector<unsigned int> indices;
    for (const Object& object : scene.getObjects()) {
        const Mesh& mesh = object.getMesh();
        const Vec3Df c = object.getMaterial().getColor();
        const unsigned int base = static_cast<unsigned int>(vertices.size());
        for (const Vertex& v : mesh.getVertices()) {
            const Vec3Df& p = v.getPos();
            const Vec3Df& n = v.getNormal();
            vertices.push_back({p[0], p[1], p[2], n[0], n[1], n[2], c[0], c[1], c[2]});
        }
        for (const Triangle& t : mesh.getTriangles()) {
            indices.push_back(base + t.getVertex(0));
            indices.push_back(base + t.getVertex(1));
            indices.push_back(base + t.getVertex(2));
        }
    }
    GLModel m;
    m.numIndices = indices.size();
    glGenVertexArrays(1, &m.vao);
    glGenBuffers(1, &m.vbo);
    glGenBuffers(1, &m.ebo);

    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GLVertex), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GLVertex), reinterpret_cast<void*>(offsetof(GLVertex, x)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GLVertex), reinterpret_cast<void*>(offsetof(GLVertex, nx)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(GLVertex), reinterpret_cast<void*>(offsetof(GLVertex, r)));
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    return m;
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

// ---- controls-panel helpers --------------------------------------------------
// Each section of the Controls panel is a table column; inside it every row is
// "muted label on the left, value or widget aligned at kLabelWidth", so all
// sections read the same way.
constexpr float kLabelWidth = 84.f;

void rowLabel(const char* label) {
    ImGui::TextDisabled("%s", label);
    ImGui::SameLine(kLabelWidth);
}

void rowValue(const char* label, const char* fmt, ...) IM_FMTARGS(2);
void rowValue(const char* label, const char* fmt, ...) {
    rowLabel(label);
    va_list args;
    va_start(args, fmt);
    ImGui::TextV(fmt, args);
    va_end(args);
}

// Label followed by a widget that fills the rest of the row.
void rowWidget(const char* label) {
    rowLabel(label);
    ImGui::SetNextItemWidth(-FLT_MIN);
}

const char* kModeLabels[] = {"Lit (Lambert)", "Ambient", "Hit mask", "Normals", "Depth", "Object id"};
const char* kModeSlugs[] = {"lit", "ambient", "hitmask", "normals", "depth", "objectid"};

}  // namespace

int main(int argc, char** argv) {
    // The model is optional: the Controls panel has a picker for every .off in
    // the models directory. A path on the command line is loaded first.
    const char* givenModel = argc >= 2 ? argv[1] : nullptr;
    if (givenModel && (std::string(givenModel) == "-h" || std::string(givenModel) == "--help")) {
        std::cout << "Usage: " << argv[0] << " [model.off|model.obj]" << std::endl;
        return 0;
    }
    const std::vector<std::filesystem::path> modelPaths = listModels(modelsDirectory(givenModel));
    std::vector<std::string> modelNames;
    for (const auto& p : modelPaths) modelNames.push_back(p.stem().string());
    if (!givenModel && modelPaths.empty()) {
        std::cerr << "No .off/.obj models found in " << RAYMINI_MODELS_DIR << " or ./models; pass one: "
                  << argv[0] << " <model.off|model.obj>" << std::endl;
        return 1;
    }
    std::string initialPath;
    if (givenModel) {
        initialPath = givenModel;
    } else {
        const auto teapot = std::find_if(modelPaths.begin(), modelPaths.end(),
                                         [](const std::filesystem::path& p) { return p.filename() == "teapot.off"; });
        initialPath = (teapot != modelPaths.end() ? *teapot : modelPaths.front()).string();
    }

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
    glfwSetWindowSizeLimits(window, kMinWindowWidth, kMinWindowHeight, GLFW_DONT_CARE, GLFW_DONT_CARE);
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

    // Offscreen framebuffer the GL preview is rendered into, then shown via
    // ImGui::Image. Its size follows the panel (ensureViewportSize), so the
    // preview stays sharp at any window size, full screen included.
    GLuint viewportFBO, viewportTexture, depthBuffer;
    int fboW = 0, fboH = 0;
    glGenFramebuffers(1, &viewportFBO);
    glGenTextures(1, &viewportTexture);
    glGenRenderbuffers(1, &depthBuffer);
    auto ensureViewportSize = [&](int w, int h) {
        w = std::max(1, w);
        h = std::max(1, h);
        if (w == fboW && h == fboH) return;
        fboW = w;
        fboH = h;
        glBindTexture(GL_TEXTURE_2D, viewportTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, w, h);
        glBindFramebuffer(GL_FRAMEBUFFER, viewportFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, viewportTexture, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    };

    // Raytracer state. `rt` holds the settings; a RenderJob copies them when a
    // render starts and traces on its own thread, so the UI never blocks.
    RayTracer rt;
    std::unique_ptr<RenderJob> renderJob;  // in-flight render, if any
    unsigned int lastUploadedTiles = 0;
    Image lastRender;  // last finished (or cancelled) render
    RayTracer::Stats lastStats;
    int lastRenderMode = 0;
    bool lastRenderCancelled = false;
    GLuint rtTexture = 0;
    int rtTexW = 0, rtTexH = 0;
    int rtResolution = 256;
    int rtMode = static_cast<int>(RayTracer::DebugMode::LIT);
    float rtDepthNear = 0.f, rtDepthFar = 10.f;  // set per model by loadModel
    std::string lastSavedPath;
    bool wireframe = false;
    bool cullBackFaces = true;  // off helps with OFF files whose winding is inconsistent

    // Scene / model state. The picker in the Controls panel swaps models at
    // runtime, so everything derived from the mesh is rebuilt by loadModel.
    Scene scene;
    GLModel glModel;
    std::string modelPath;
    std::string loadError;
    int modelIndex = -1;  // index into modelPaths; -1 if the current file is not in the list

    // Load `path`: new scene, new GL buffers, camera re-framed, previous render
    // discarded. On failure the previous model stays and the error is shown.
    auto loadModel = [&](const std::string& path) -> bool {
        Scene next;
        try {
            next.addObjectsFromFile(path);
        } catch (const std::exception& e) {
            loadError = e.what();
            std::cerr << loadError << std::endl;
            return false;
        }
        next.addDefaultLights();
        scene = std::move(next);
        modelPath = path;
        loadError.clear();

        destroyModel(glModel);
        glModel = createModel(scene);
        frameModel(scene.getBoundingBox());
        // Depth mode defaults: the model spans initialDistance +- size from the camera.
        const float modelSize = scene.getBoundingBox().getSize();
        rtDepthNear = std::max(0.f, initialDistance - modelSize);
        rtDepthFar = initialDistance + modelSize;
        renderJob.reset();  // a render of the previous model is meaningless now
        lastRender = Image();
        lastRenderCancelled = false;
        lastSavedPath.clear();

        modelIndex = -1;
        for (size_t i = 0; i < modelPaths.size(); ++i) {
            std::error_code ec;
            if (std::filesystem::equivalent(modelPaths[i], path, ec)) {
                modelIndex = static_cast<int>(i);
                break;
            }
        }
        size_t nv = 0, nt = 0;
        for (const Object& o : scene.getObjects()) {
            nv += o.getMesh().getVertices().size();
            nt += o.getMesh().getTriangles().size();
        }
        std::cout << "Loaded " << path << ": " << scene.getObjects().size() << " object(s), " << nv
                  << " vertices, " << nt << " triangles" << std::endl;
        glfwSetWindowTitle(window, ("Raymini - " + std::filesystem::path(path).filename().string()).c_str());
        return true;
    };

    if (!loadModel(initialPath)) {
        // A bad path on the command line falls back to the bundled models but
        // keeps telling the user what went wrong with theirs.
        const std::string firstError = loadError;
        bool loaded = false;
        for (const auto& p : modelPaths) {
            if (loadModel(p.string())) {
                loaded = true;
                break;
            }
        }
        if (!loaded) {
            std::cerr << "Could not load any model." << std::endl;
            return 1;
        }
        loadError = firstError;
    }

    const ImGuiWindowFlags panelFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ---- layout: follows the window, so resizing and full screen work ----
        const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
        const ImVec2 areaPos = mainViewport->WorkPos;
        const ImVec2 areaSize = mainViewport->WorkSize;
        const float topHeight = std::max(kMinTopHeight, areaSize.y - kControlsHeight - 3.f * kMargin);
        const float leftWidth = std::max(1.f, std::floor((areaSize.x - 3.f * kMargin) * kPreviewShare));
        const float rightWidth = std::max(1.f, areaSize.x - 3.f * kMargin - leftWidth);
        int displayW, displayH;
        glfwGetFramebufferSize(window, &displayW, &displayH);
        const float pixelScale = areaSize.x > 0.f ? static_cast<float>(displayW) / areaSize.x : 1.f;

        // ---- GL preview -----------------------------------------------------
        ImGui::SetNextWindowPos(ImVec2(areaPos.x + kMargin, areaPos.y + kMargin), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(leftWidth, topHeight), ImGuiCond_Always);
        ImGui::Begin("OpenGL Viewer", nullptr, panelFlags);
        // Fit a 3:2 image in the panel and render the preview at that size in
        // framebuffer pixels, so it is sharp on any window and any display.
        const ImVec2 previewSize = fitAspect(ImGui::GetContentRegionAvail(), kViewportAspect);
        ensureViewportSize(static_cast<int>(previewSize.x * pixelScale), static_cast<int>(previewSize.y * pixelScale));

        glBindFramebuffer(GL_FRAMEBUFFER, viewportFBO);
        glViewport(0, 0, fboW, fboH);
        glClearColor(0.f, 0.f, 0.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
        if (cullBackFaces)
            glEnable(GL_CULL_FACE);
        else
            glDisable(GL_CULL_FACE);
        glUseProgram(shaderProgram);

        cameraFront = glm::normalize(cameraTarget - cameraPos);
        const glm::mat4 projection = glm::perspective(glm::radians(fov), kViewportAspect, 0.01f * initialDistance, 100.f * initialDistance);
        const glm::mat4 view = glm::lookAt(cameraPos, cameraTarget, cameraUp);
        const glm::mat4 model(1.f);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(cameraPosLoc, 1, glm::value_ptr(cameraPos));

        glBindVertexArray(glModel.vao);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(glModel.numIndices), GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // A framebuffer texture stores its first row at the bottom of the scene,
        // so flip V (uv0 = top-left = 0,1) to show the preview upright, matching
        // the raytraced panel.
        ImGui::Image(static_cast<ImTextureID>(viewportTexture), previewSize, ImVec2(0, 1), ImVec2(1, 0));
        viewerHovered = ImGui::IsItemHovered();
        ImGui::End();

        // ---- raytracer panel -------------------------------------------------
        ImGui::SetNextWindowPos(ImVec2(areaPos.x + 2.f * kMargin + leftWidth, areaPos.y + kMargin), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(rightWidth, topHeight), ImGuiCond_Always);
        ImGui::Begin("Raytracer", nullptr, panelFlags);

        // Settings live in the Render section of the Controls panel; this
        // panel holds the actions, the progress and the result. Rendering
        // runs on a worker thread (RenderJob), so the UI stays live and the
        // image fills in tile by tile.
        auto showRenderImage = [&]() {
            // Fit the render into the panel without stretching (its rect keeps
            // the rendered image's aspect, which is the preview's aspect).
            const float imgAspect = static_cast<float>(rtTexW) / static_cast<float>(std::max(1, rtTexH));
            ImGui::Image(static_cast<ImTextureID>(rtTexture), fitAspect(ImGui::GetContentRegionAvail(), imgAspect));
        };

        if (renderJob) {
            if (ImGui::Button("Cancel")) renderJob->cancel();
        } else if (ImGui::Button("Render Scene")) {
            auto toVec3Df = [](const glm::vec3& v) { return Vec3Df(v.x, v.y, v.z); };
            // Match the GL preview exactly: same eye/target/up, same vertical
            // fov and the same aspect ratio, so the framing is identical. The
            // resolution slider sets the width; the height follows the aspect.
            const int renderW = rtResolution;
            const int renderH = renderHeightFor(rtResolution);
            const Camera camera = Camera::lookAt(toVec3Df(cameraPos), toVec3Df(cameraTarget), toVec3Df(cameraUp),
                                                 glm::radians(fov), kViewportAspect);
            rt.setDebugMode(static_cast<RayTracer::DebugMode>(rtMode));
            rt.setDepthRange(rtDepthNear, rtDepthFar);
            // The job copies tracer, scene and camera: editing them meanwhile is safe.
            renderJob = std::make_unique<RenderJob>(rt, scene, camera, renderW, renderH, kRenderTileSize,
                                                    Vec3Df(0.12f, 0.12f, 0.12f));
            renderJob->start();
            lastUploadedTiles = 0;
            lastRender = Image();
            lastRenderMode = rtMode;
            lastRenderCancelled = false;
            lastSavedPath.clear();
            uploadTexture(rtTexture, rtTexW, rtTexH, renderJob->snapshot());  // pending fill
        }

        if (renderJob) {
            // Pull what the worker finished since the last frame.
            const unsigned int done = renderJob->completedTiles();
            const bool finished = renderJob->isDone();
            if (done != lastUploadedTiles || finished) {
                uploadTexture(rtTexture, rtTexW, rtTexH, renderJob->snapshot());
                lastUploadedTiles = done;
            }
            if (finished) {
                lastRender = renderJob->snapshot();
                lastStats = renderJob->stats();
                lastRenderCancelled = renderJob->isCancelled();
                renderJob.reset();
            }
        }

        if (renderJob) {
            ImGui::SameLine();
            char label[64];
            std::snprintf(label, sizeof(label), "%.0f%%  %.1f s", 100.f * renderJob->progress(),
                          renderJob->elapsedSeconds());
            ImGui::ProgressBar(renderJob->progress(), ImVec2(-FLT_MIN, 0.f), label);
            showRenderImage();
        } else if (lastRender.isValid()) {
            ImGui::SameLine();
            if (ImGui::Button("Save PNG")) {
                std::filesystem::create_directories("renders");
                char name[128];
                std::snprintf(name, sizeof(name), "renders/render_%s_%dx%d.png", kModeSlugs[lastRenderMode], rtTexW,
                              rtTexH);
                lastSavedPath = lastRender.save(name) ? name : "save failed";
            }
            ImGui::SameLine();
            if (lastRenderCancelled) {
                // Stats only cover published tiles, so rays / pixels is the share done.
                ImGui::Text("%dx%d cancelled after %.2fs (%.0f%% done)", rtTexW, rtTexH, lastStats.seconds,
                            100.0 * static_cast<double>(lastStats.rays) / (static_cast<double>(rtTexW) * rtTexH));
            } else {
                ImGui::Text("%dx%d in %.2fs, %.0f%% hits", rtTexW, rtTexH, lastStats.seconds,
                            lastStats.rays ? 100.0 * lastStats.hits / lastStats.rays : 0.0);
            }
            if (!lastSavedPath.empty()) {
                ImGui::SameLine();
                ImGui::TextDisabled("%s", lastSavedPath.c_str());
            }
            showRenderImage();
        } else {
            ImGui::SameLine();
            ImGui::TextDisabled("(no render yet)");
        }
        ImGui::End();

        // ---- controls ---------------------------------------------------------
        ImGui::SetNextWindowPos(ImVec2(areaPos.x + kMargin, areaPos.y + 2.f * kMargin + topHeight), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(std::max(1.f, areaSize.x - 2.f * kMargin), kControlsHeight), ImGuiCond_Always);
        ImGui::Begin("Controls", nullptr, panelFlags);
        // Four sections, one per thing a setting affects: the model, the camera
        // both views share, the GL preview, and the raytraced render. Sections
        // are separated by vertical rules and read as "label | value" rows.
        const ImGuiTableFlags sectionFlags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV |
                                             ImGuiTableFlags_PadOuterX | ImGuiTableFlags_NoSavedSettings;
        if (ImGui::BeginTable("sections", 4, sectionFlags)) {
            ImGui::TableSetupColumn("Model", ImGuiTableColumnFlags_WidthStretch, 1.2f);
            ImGui::TableSetupColumn("Camera", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthStretch, 0.7f);
            ImGui::TableSetupColumn("Render", ImGuiTableColumnFlags_WidthStretch, 1.1f);
            ImGui::TableNextRow();

            // ---- Model ----
            ImGui::TableNextColumn();
            ImGui::SeparatorText("Model");
            if (modelNames.empty()) {
                ImGui::TextDisabled("(no models directory found)");
            } else {
                // Picker over every .off in the models directory. If a load fails
                // modelIndex is untouched, so the combo snaps back next frame.
                std::vector<const char*> names;
                names.reserve(modelNames.size());
                for (const std::string& n : modelNames) names.push_back(n.c_str());
                rowWidget("File");
                int pick = modelIndex;
                if (ImGui::Combo("##model", &pick, names.data(), static_cast<int>(names.size())) && pick >= 0 &&
                    pick != modelIndex) {
                    loadModel(modelPaths[static_cast<size_t>(pick)].string());
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", modelPath.c_str());
            }
            if (!loadError.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.4f, 0.4f, 1.f));
                ImGui::TextWrapped("%s", loadError.c_str());
                ImGui::PopStyleColor();
            }
            {
                size_t nv = 0, nt = 0;
                for (const Object& o : scene.getObjects()) {
                    nv += o.getMesh().getVertices().size();
                    nt += o.getMesh().getTriangles().size();
                }
                const BoundingBox& bbox = scene.getBoundingBox();
                rowValue("Objects", "%zu", scene.getObjects().size());
                rowValue("Vertices", "%zu", nv);
                rowValue("Triangles", "%zu", nt);
                rowValue("Bounds", "%.2f x %.2f x %.2f", bbox.getWidth(), bbox.getHeight(), bbox.getLength());
            }

            // ---- Camera ----
            ImGui::TableNextColumn();
            ImGui::SeparatorText("Camera");
            rowWidget("FOV");
            ImGui::SliderFloat("##fov", &fov, 10.0f, 120.0f, "%.0f deg");
            rowValue("Position", "%.2f, %.2f, %.2f", cameraPos.x, cameraPos.y, cameraPos.z);
            rowValue("Target", "%.2f, %.2f, %.2f", cameraTarget.x, cameraTarget.y, cameraTarget.z);
            rowValue("Distance", "%.2f", glm::length(cameraPos - cameraTarget));
            if (ImGui::Button("Reset Camera")) resetCamera();

            // ---- Preview ----
            ImGui::TableNextColumn();
            ImGui::SeparatorText("Preview");
            ImGui::Checkbox("Wireframe", &wireframe);
            ImGui::Checkbox("Cull back faces", &cullBackFaces);
            ImGui::Spacing();
            ImGui::TextDisabled("Left-drag to orbit");
            ImGui::TextDisabled("Scroll to zoom");

            // ---- Render ----
            ImGui::TableNextColumn();
            ImGui::SeparatorText("Render");
            rowWidget("Width");
            ImGui::SliderInt("##width", &rtResolution, 64, 1024, "%d px");
            rowValue("Output", "%d x %d px", rtResolution, renderHeightFor(rtResolution));
            rowWidget("Mode");
            ImGui::Combo("##mode", &rtMode, kModeLabels, IM_ARRAYSIZE(kModeLabels));
            if (static_cast<RayTracer::DebugMode>(rtMode) == RayTracer::DebugMode::DEPTH) {
                rowWidget("Near");
                ImGui::SliderFloat("##near", &rtDepthNear, 0.f, 10.f * initialDistance, "%.2f");
                rowWidget("Far");
                ImGui::SliderFloat("##far", &rtDepthFar, 0.f, 10.f * initialDistance, "%.2f");
            }

            ImGui::EndTable();
        }
        ImGui::End();

        // ---- present ------------------------------------------------------------
        ImGui::Render();
        glViewport(0, 0, displayW, displayH);
        glClearColor(0.f, 0.f, 0.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    destroyModel(glModel);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
