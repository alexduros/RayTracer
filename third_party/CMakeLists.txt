# Third-party code vendored into the repo. Each library is its own target so
# raymini_core stays free of any GL / windowing dependency.

# stb_image / stb_image_write (header-only). The implementation is compiled
# exactly once, in src/core/Image.cpp.
add_library(stb INTERFACE)
target_include_directories(stb SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

if(RAYMINI_BUILD_GUI)
  # glad: OpenGL 3.3 core loader.
  add_library(glad STATIC glad/gl.c)
  target_include_directories(glad
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}        # users write #include <glad/gl.h>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/glad)  # gl.c itself does #include "gl.h"

  # Dear ImGui 1.91.5, core + GLFW/OpenGL3 backends only.
  add_library(imgui STATIC
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/backends/imgui_impl_glfw.cpp
    imgui/backends/imgui_impl_opengl3.cpp)
  target_include_directories(imgui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends)
  target_link_libraries(imgui PUBLIC glfw)
  target_compile_definitions(imgui PUBLIC GL_SILENCE_DEPRECATION)
endif()
