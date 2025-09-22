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

# Include directories
include_directories(/usr/local/include)
include_directories(/usr/local/Cellar/glfw/3.4/include)
include_directories(/usr/local/Cellar/glm/1.0.1/include)
include_directories(${CMAKE_SOURCE_DIR}/glad)
include_directories(${CMAKE_SOURCE_DIR}/stb)
include_directories(${CMAKE_SOURCE_DIR}/imgui)
include_directories(${CMAKE_SOURCE_DIR}/imgui/backends)

# Link libraries
link_directories(/usr/local/lib)

# Add the executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Find and include glm
find_package(glm REQUIRED)
include_directories(${GLM_INCLUDE_DIRS})

# Link the GLFW libraries
target_link_libraries(${PROJECT_NAME} glfw "-framework OpenGL" glm::glm)