# Single test binary with a tiny built-in harness (tests/Test.h): no external
# dependency. Run all with `ctest --test-dir build`, or pick cases with
# `build/raymini_tests --filter <substring>`.
add_executable(raymini_tests
  TestMain.cpp
  TestVec3D.cpp
  TestMesh.cpp
  TestRay.cpp
  TestBvh.cpp
  TestCamera.cpp
  TestRender.cpp
  TestImage.cpp
  TestObj.cpp
  TestRenderJob.cpp
  TestAntiAliasing.cpp
  TestShading.cpp
  TestReflection.cpp
  TestOcclusion.cpp
  TestRefraction.cpp
  TestDisplay.cpp
  TestPrimitive.cpp
  TestTexture.cpp
  TestOrientation.cpp
  TestGolden.cpp)
target_link_libraries(raymini_tests PRIVATE raymini_core)
target_compile_definitions(raymini_tests PRIVATE
  RAYMINI_MODELS_DIR="${RAYMINI_MODELS_DIR}"
  RAYMINI_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden"
  RAYMINI_TEST_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}/out")
target_compile_options(raymini_tests PRIVATE ${RAYMINI_WARNINGS})

add_test(NAME unit   COMMAND raymini_tests --exclude golden)
add_test(NAME golden COMMAND raymini_tests --filter golden)
# The binary reports the version CMake built it with: what the release
# archive is named after (scripts/package.sh checks the same thing).
add_test(NAME cli_version COMMAND raymini-cli --version)
set_tests_properties(cli_version PROPERTIES PASS_REGULAR_EXPRESSION "^raymini ${PROJECT_VERSION}
$")

add_test(NAME cli_smoke COMMAND raymini-cli teapot --size 32x32 --quiet
         --out ${CMAKE_CURRENT_BINARY_DIR}/out/cli_smoke.png)
add_test(NAME cli_soft_shadows COMMAND raymini-cli teapot --size 32x32 --ground --shadow-samples 2
         --light-radius 0.2 --quiet --out ${CMAKE_CURRENT_BINARY_DIR}/out/cli_soft_shadows.png)
add_test(NAME cli_reflections COMMAND raymini-cli teapot --size 32x32 --ground-reflectivity 0.5
         --reflectivity 0.2 --max-depth 2 --quiet --out ${CMAKE_CURRENT_BINARY_DIR}/out/cli_reflections.png)
add_test(NAME cli_ao COMMAND raymini-cli teapot --size 32x32 --ground --ao 2 --ao-radius 0.3 --quiet
         --out ${CMAKE_CURRENT_BINARY_DIR}/out/cli_ao.png)
add_test(NAME cli_ao_mode COMMAND raymini-cli teapot --size 32x32 --mode ao --quiet
         --out ${CMAKE_CURRENT_BINARY_DIR}/out/cli_ao_mode.png)
add_test(NAME cli_glass COMMAND raymini-cli ram --size 32x32 --ground --transparency 1 --ior 1.33
         --max-depth 6 --quiet --out ${CMAKE_CURRENT_BINARY_DIR}/out/cli_glass.png)
add_test(NAME cli_bad_ior COMMAND raymini-cli ram --ior 0.5 --quiet)
set_tests_properties(cli_bad_ior PROPERTIES WILL_FAIL TRUE)
add_test(NAME cli_display COMMAND raymini-cli teapot --size 32x32 --display linear --tonemap reinhard --white 4
         --exposure 1 --gamma 2.2 --quiet --out ${CMAKE_CURRENT_BINARY_DIR}/out/cli_display.png)
add_test(NAME cli_hdr COMMAND raymini-cli teapot --size 32x32 --quiet --out ${CMAKE_CURRENT_BINARY_DIR}/out/cli.hdr)
add_test(NAME cli_overrides COMMAND raymini-cli teapot --size 32x32 --light 0 3 3 3 1 0.5 0.2 1
         --light 3 0 4 0 1 1 1 0.5 --ambient 0.05 --color 0.2 0.4 0.9 --specular 0.5 --shininess 64 --quiet
         --out ${CMAKE_CURRENT_BINARY_DIR}/out/cli_overrides.png)
add_test(NAME cli_bad_tonemap COMMAND raymini-cli teapot --tonemap filmic --quiet)
set_tests_properties(cli_bad_tonemap PROPERTIES WILL_FAIL TRUE)
add_test(NAME cli_bad_light COMMAND raymini-cli teapot --light 5 0 1 0 1 1 1 1 --quiet)
set_tests_properties(cli_bad_light PROPERTIES WILL_FAIL TRUE)
add_test(NAME cli_texture COMMAND raymini-cli spot --size 32x32 --ground --texture
         ${CMAKE_CURRENT_SOURCE_DIR}/../models/spot_texture.png --quiet
         --out ${CMAKE_CURRENT_BINARY_DIR}/out/cli_texture.png)
add_test(NAME cli_uv_mode COMMAND raymini-cli spot --size 32x32 --mode uv --quiet
         --out ${CMAKE_CURRENT_BINARY_DIR}/out/cli_uv.png)
add_test(NAME cli_threads COMMAND raymini-cli teapot --size 32x32 --threads 3 --quiet
         --out ${CMAKE_CURRENT_BINARY_DIR}/out/cli_threads.png)
add_test(NAME cli_bad_threads COMMAND raymini-cli teapot --threads many --quiet)
set_tests_properties(cli_bad_threads PROPERTIES WILL_FAIL TRUE)
add_test(NAME cli_missing_model COMMAND raymini-cli does_not_exist.off --quiet)
set_tests_properties(cli_missing_model PROPERTIES WILL_FAIL TRUE)
