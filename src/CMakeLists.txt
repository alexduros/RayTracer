cmake_minimum_required(VERSION 3.10)

# Project name
project(raymini)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Specify the compiler
set(CMAKE_C_COMPILER /usr/bin/clang)
set(CMAKE_CXX_COMPILER /usr/bin/clang++)

# Define the source files
set(SOURCES
    Main.cpp
    glad/gl.c
)

# Define the header files
set(HEADERS
    glad/gl.h
    glad/KHR/khrplatform.h
)

# Include directories
include_directories(/usr/local/include)
include_directories(${CMAKE_SOURCE_DIR}/glad)

# Link libraries
link_directories(/usr/local/lib)

# Add the executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Find and include glm
find_package(glm REQUIRED)
include_directories(${GLM_INCLUDE_DIRS})

# Link the GLFW libraries
target_link_libraries(${PROJECT_NAME} glfw "-framework OpenGL" glm::glm)