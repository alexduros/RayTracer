cmake_minimum_required(VERSION 3.16)
project(raymini LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})   # all executables in build/

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(RAYMINI_BUILD_GUI "Build the GLFW/Dear ImGui viewer (needs glfw3 and glm)" ON)
option(RAYMINI_BUILD_TESTS "Build the unit and golden-image tests" ON)

if(MSVC)
  set(RAYMINI_WARNINGS /W4)
else()
  set(RAYMINI_WARNINGS -Wall -Wextra)
endif()

if(RAYMINI_BUILD_GUI)
  find_package(OpenGL REQUIRED)
  find_package(glfw3 3.3 REQUIRED)
  find_package(glm QUIET)
  if(NOT TARGET glm::glm)
    # Older distro packages ship glm without a CMake config; it is header-only.
    find_path(GLM_INCLUDE_DIR glm/glm.hpp REQUIRED)
    add_library(glm::glm INTERFACE IMPORTED)
    target_include_directories(glm::glm INTERFACE ${GLM_INCLUDE_DIR})
  endif()
endif()

# Absolute path to the bundled OFF models; baked into the tests.
set(RAYMINI_MODELS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/models")

add_subdirectory(third_party)
add_subdirectory(src)

if(RAYMINI_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
