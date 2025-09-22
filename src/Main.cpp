#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "glad/gl.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <locale>
#include <cmath>

// Dear ImGui
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"


using namespace std;

struct Vertex {
    float x, y, z;
    float r, g, b;
};

struct Face {
    std::vector<unsigned int> vertices;
};

glm::vec3 cameraPos(0.0f, 0.0f, 5.0f);
glm::vec3 cameraFront(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp(0.0f, 1.0f, 0.0f);
glm::vec3 cameraTarget(0.0f, 0.0f, 0.0f);
glm::vec3 cameraDirection = glm::normalize(cameraPos - cameraTarget);
glm::vec3 cameraRight = glm::normalize(glm::cross(cameraUp, cameraDirection));
glm::vec3 cameraUpDirection = glm::cross(cameraDirection, cameraRight);
float yaw = -90.0f;
float pitch = 0.0f;
float lastX = 400, lastY = 300;
bool firstMouse = true;
bool leftMouseButtonPressed = false;
float fov = 45.0f;

static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

bool loadOFF(const std::string& filename, std::vector<Vertex>& vertices, std::vector<Face>& faces) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return false;
    }

    std::string line;
    std::getline(file, line);
    rtrim(line);
    if (line != "OFF") {
        std::cerr << "Not a valid OFF file: " << filename << std::endl;
        return false;
    }

    unsigned int numVertices, numFaces, numEdges;
    file >> numVertices >> numFaces >> numEdges;

    vertices.resize(numVertices);

    // Read all vertices first
    for (unsigned int i = 0; i < numVertices; ++i) {
        file >> vertices[i].x >> vertices[i].y >> vertices[i].z;
    }

    // Calculate bounds for color normalization
    float minY = vertices[0].y, maxY = vertices[0].y;
    for (const auto& v : vertices) {
        minY = std::min(minY, v.y);
        maxY = std::max(maxY, v.y);
    }

    // Add color variation based on normalized Y position
    for (unsigned int i = 0; i < numVertices; ++i) {
        float normalizedY = (maxY != minY) ? (vertices[i].y - minY) / (maxY - minY) : 0.5f;
        vertices[i].r = 0.8f + 0.2f * normalizedY; // Red gradient
        vertices[i].g = 0.6f + 0.4f * (1.0f - normalizedY); // Green gradient
        vertices[i].b = 0.4f + 0.6f * normalizedY; // Blue gradient
    }

    faces.resize(numFaces);
    for (unsigned int i = 0; i < numFaces; ++i) {
        unsigned int numVerts;
        file >> numVerts;
        faces[i].vertices.resize(numVerts);
        for (unsigned int j = 0; j < numVerts; ++j) {
            file >> faces[i].vertices[j];
        }
    }

    return true;
}

GLuint createModel(const std::vector<Vertex>& vertices, const std::vector<Face>& faces) {
    GLuint vao, vbo, ebo;
    std::vector<unsigned int> indices;

    // Calculate model bounds
    glm::vec3 minBounds(vertices[0].x, vertices[0].y, vertices[0].z);
    glm::vec3 maxBounds(vertices[0].x, vertices[0].y, vertices[0].z);

    for (const auto& vertex : vertices) {
        minBounds.x = std::min(minBounds.x, vertex.x);
        minBounds.y = std::min(minBounds.y, vertex.y);
        minBounds.z = std::min(minBounds.z, vertex.z);
        maxBounds.x = std::max(maxBounds.x, vertex.x);
        maxBounds.y = std::max(maxBounds.y, vertex.y);
        maxBounds.z = std::max(maxBounds.z, vertex.z);
    }

    glm::vec3 modelCenter = (minBounds + maxBounds) * 0.5f;
    glm::vec3 modelSize = maxBounds - minBounds;
    float maxDimension = std::max({modelSize.x, modelSize.y, modelSize.z});

    // Adjust camera to frame the model nicely
    cameraTarget = modelCenter;
    float distance = maxDimension * 2.0f; // Distance factor for good framing
    cameraPos = modelCenter + glm::vec3(0.0f, 0.0f, distance);

    std::cout << "Model bounds: min(" << minBounds.x << "," << minBounds.y << "," << minBounds.z << ")"
              << " max(" << maxBounds.x << "," << maxBounds.y << "," << maxBounds.z << ")" << std::endl;
    std::cout << "Model center: (" << modelCenter.x << "," << modelCenter.y << "," << modelCenter.z << ")" << std::endl;
    std::cout << "Camera distance: " << distance << std::endl;

    for (const auto& face : faces) {
        for (unsigned int index : face.vertices) {
            indices.push_back(index);
        }
    }

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return vao;
}

void renderModel(GLuint vao, size_t numIndices) {
    cout << "Raymini: render model..." << endl;
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    cout << "Raymini: framebuffer_size_callback " << width << height << endl;
    glViewport(0, 0, width, height);
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            leftMouseButtonPressed = true;
        } else if (action == GLFW_RELEASE) {
            leftMouseButtonPressed = false;
            firstMouse = true;
        }
    }
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (!leftMouseButtonPressed) {
        return;
    }

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    if (pitch > 89.0f)
        pitch = 89.0f;
    if (pitch < -89.0f)
        pitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);

    // Update camera position to rotate around the target point
    cameraPos = cameraTarget - cameraFront * glm::length(cameraPos - cameraTarget);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    fov -= yoffset;
}


const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
out vec3 ourColor;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    ourColor = aColor;
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
in vec3 ourColor;
out vec4 FragColor;
void main() {
    FragColor = vec4(ourColor, 1.0);
}
)";

GLuint createShaderProgram() {
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    GLint success;
    GLchar infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

int main (int argc, char **argv)
{
    cout << "Raymini: Entering main..." << endl;

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <base_path>" << std::endl;
        return -1;
    }

    if (!glfwInit())
        return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE); // Required on macOS

    cout << "Raymini: Creates main window..." << endl;
    int width = 1400;  // Wider for dual viewport
    int height = 800;

    GLFWwindow* window = glfwCreateWindow(width, height, "Raymini", NULL, NULL);
    if (!window)
    {
        cerr << "Failed to create GLFW window" << endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);


    if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress)) {
        cerr << "Failed to initialize GLAD" << endl;
        return -1;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    std::string modelPath = argv[1];
    std::vector<Vertex> vertices;
    std::vector<Face> faces;

    if (!loadOFF(modelPath, vertices, faces)) {
        return -1;
    }
    GLuint vao = createModel(vertices, faces);
    size_t numIndices = faces.size() * 3;

    GLuint shaderProgram = createShaderProgram();
    glUseProgram(shaderProgram);

    GLint modelLoc = glGetUniformLocation(shaderProgram, "model");
    GLint viewLoc = glGetUniformLocation(shaderProgram, "view");
    GLint projectionLoc = glGetUniformLocation(shaderProgram, "projection");

    glClearColor (0.f, 0.f, 0.f, 0.0);
    glCullFace (GL_BACK);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // Create framebuffer for GL viewport
    GLuint glViewportFBO, glViewportTexture;
    glGenFramebuffers(1, &glViewportFBO);
    glGenTextures(1, &glViewportTexture);

    glBindTexture(GL_TEXTURE_2D, glViewportTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 600, 400, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindFramebuffer(GL_FRAMEBUFFER, glViewportFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, glViewportTexture, 0);

    // Create depth buffer
    GLuint depthBuffer;
    glGenRenderbuffers(1, &depthBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, 600, 400);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuffer);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Simple side-by-side layout without docking

        // GL Viewport Panel - Fixed position and size
        ImGui::SetNextWindowPos(ImVec2(10, 20), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(620, 460), ImGuiCond_Always);
        ImGuiWindowFlags gl_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
        ImGui::Begin("OpenGL Viewer", nullptr, gl_flags);

        // Render to framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, glViewportFBO);
        glViewport(0, 0, 600, 400);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Use shader program
        glUseProgram(shaderProgram);

        // Set up matrices
        glm::mat4 projection = glm::perspective(glm::radians(fov), 600.0f / 400.0f, 0.1f, 100.0f);
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        glm::mat4 model = glm::mat4(1.0f);

        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));

        renderModel(vao, numIndices);

        // Back to default framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Display the texture in ImGui
        ImGui::Image((ImTextureID)(intptr_t)glViewportTexture, ImVec2(600, 400));
        ImGui::End();

        // Raytracer Panel - Fixed position and size
        ImGui::SetNextWindowPos(ImVec2(650, 20), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(730, 460), ImGuiCond_Always);
        ImGuiWindowFlags rt_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
        ImGui::Begin("Raytracer", nullptr, rt_flags);
        ImGui::Text("Raytracer rendering will go here");
        if (ImGui::Button("Render Scene")) {
            ImGui::Text("Rendering...");
        }
        ImGui::End();

        // Camera Controls Panel - Fixed position and size
        ImGui::SetNextWindowPos(ImVec2(10, 500), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(1370, 280), ImGuiCond_Always);
        ImGuiWindowFlags ctrl_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
        ImGui::Begin("Controls", nullptr, ctrl_flags);

        // Split controls into columns
        ImGui::Columns(3, "ControlColumns", true);

        // Camera Controls Column
        ImGui::Text("Camera Controls");
        ImGui::Separator();
        ImGui::SliderFloat("FOV", &fov, 10.0f, 120.0f);
        ImGui::Text("Position: %.2f, %.2f, %.2f", cameraPos.x, cameraPos.y, cameraPos.z);
        ImGui::Text("Target: %.2f, %.2f, %.2f", cameraTarget.x, cameraTarget.y, cameraTarget.z);

        if (ImGui::Button("Reset Camera")) {
            // Reset to default view
            fov = 45.0f;
        }

        ImGui::NextColumn();

        // Rendering Settings Column
        ImGui::Text("Rendering Settings");
        ImGui::Separator();
        static bool wireframe = false;
        if (ImGui::Checkbox("Wireframe", &wireframe)) {
            glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
        }

        static bool showNormals = false;
        ImGui::Checkbox("Show Normals", &showNormals);

        static float lightIntensity = 1.0f;
        ImGui::SliderFloat("Light Intensity", &lightIntensity, 0.0f, 2.0f);

        ImGui::NextColumn();

        // Model Info Column
        ImGui::Text("Model Information");
        ImGui::Separator();
        ImGui::Text("Vertices: %zu", vertices.size());
        ImGui::Text("Faces: %zu", faces.size());
        ImGui::Text("Model: %s", modelPath.c_str());

        ImGui::Columns(1); // Reset to single column
        ImGui::End();

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup ImGui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();
    return 0;
}

