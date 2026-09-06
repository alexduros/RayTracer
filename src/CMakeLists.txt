# ---------------------------------------------------------------------------
# raymini_core: the raytracer. No windowing or GL dependency, so the same
# library is linked into the GUI, the headless CLI and the tests.
# ---------------------------------------------------------------------------
add_library(raymini_core STATIC
  core/Camera.cpp
  core/Image.cpp
  core/Mesh.cpp
  core/Object.cpp
  core/ObjLoader.cpp
  core/Ray.cpp
  core/RayTracer.cpp
  core/RenderJob.cpp
  core/Scene.cpp
  core/Triangle.cpp
  core/Vertex.cpp)
target_include_directories(raymini_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/core)
target_link_libraries(raymini_core PUBLIC stb Threads::Threads)  # RenderJob runs on a std::thread
target_compile_options(raymini_core PRIVATE ${RAYMINI_WARNINGS})

# ---------------------------------------------------------------------------
# raymini-cli: headless renderer, OFF in -> PNG out. Used by the tests and CI.
# ---------------------------------------------------------------------------
add_executable(raymini-cli cli/Main.cpp)
target_link_libraries(raymini-cli PRIVATE raymini_core)
target_compile_definitions(raymini-cli PRIVATE RAYMINI_MODELS_DIR="${RAYMINI_MODELS_DIR}")
target_compile_options(raymini-cli PRIVATE ${RAYMINI_WARNINGS})

# ---------------------------------------------------------------------------
# raymini: GLFW + Dear ImGui viewer with the raytracer panel.
# ---------------------------------------------------------------------------
if(RAYMINI_BUILD_GUI)
  add_executable(raymini gui/Main.cpp)
  target_link_libraries(raymini PRIVATE raymini_core imgui glad glfw OpenGL::GL glm::glm)
  target_compile_definitions(raymini PRIVATE
    GLFW_INCLUDE_NONE
    RAYMINI_MODELS_DIR="${RAYMINI_MODELS_DIR}")
  target_compile_options(raymini PRIVATE ${RAYMINI_WARNINGS})
endif()
