# Single test binary with a tiny built-in harness (tests/Test.h): no external
# dependency. Run all with `ctest --test-dir build`, or pick cases with
# `build/raymini_tests --filter <substring>`.
add_executable(raymini_tests
  TestMain.cpp
  TestVec3D.cpp
  TestMesh.cpp
  TestRay.cpp
  TestCamera.cpp
  TestRender.cpp
  TestImage.cpp
  TestObj.cpp
  TestRenderJob.cpp
  TestAntiAliasing.cpp
  TestShading.cpp
  TestGolden.cpp)
target_link_libraries(raymini_tests PRIVATE raymini_core)
target_compile_definitions(raymini_tests PRIVATE
  RAYMINI_MODELS_DIR="${RAYMINI_MODELS_DIR}"
  RAYMINI_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden"
  RAYMINI_TEST_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}/out")
target_compile_options(raymini_tests PRIVATE ${RAYMINI_WARNINGS})

add_test(NAME unit   COMMAND raymini_tests --exclude golden)
add_test(NAME golden COMMAND raymini_tests --filter golden)
add_test(NAME cli_smoke COMMAND raymini-cli teapot --size 32x32 --quiet
         --out ${CMAKE_CURRENT_BINARY_DIR}/out/cli_smoke.png)
add_test(NAME cli_missing_model COMMAND raymini-cli does_not_exist.off --quiet)
set_tests_properties(cli_missing_model PROPERTIES WILL_FAIL TRUE)
