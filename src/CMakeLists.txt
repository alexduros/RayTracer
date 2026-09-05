cmake_minimum_required(VERSION 3.10)

# Project name
project(raymini)

# Export compile commands for VSCode IntelliSense
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Silence OpenGL deprecation warnings on macOS
add_definitions(-DGL_SILENCE_DEPRECATION)

# Specify the compiler
set(CMAKE_C_COMPILER /usr/bin/clang)
set(CMAKE_CXX_COMPILER /usr/bin/clang++)

# Define the source files
set(SOURCES
    Main.cpp
    Image.cpp
    RayTracer.cpp
    Scene.cpp
    BoundingBox.cpp
    KdTree.cpp
    Light.cpp
    Material.cpp
    Mesh.cpp
    Object.cpp
    Ray.cpp
    Triangle.cpp
    TriangleC.cpp
    Vertex.cpp
    glad/gl.c
    # Dear ImGui
    imgui/imgui.cpp
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/backends/imgui_impl_glfw.cpp
    imgui/backends/imgui_impl_opengl3.cpp
)

# Define the header files
set(HEADERS
    glad/gl.h
    glad/KHR/khrplatform.h
)

# Find dependencies via pkg-config / homebrew. Works on both Intel (/usr/local)
# and Apple Silicon (/opt/homebrew) as long as `brew install glfw glm` ran.
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/glad)
include_directories(${CMAKE_SOURCE_DIR}/stb)
include_directories(${CMAKE_SOURCE_DIR}/imgui)
include_directories(${CMAKE_SOURCE_DIR}/imgui/backends)

# Add the executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_link_libraries(${PROJECT_NAME} PRIVATE glfw OpenGL::GL glm::glm)