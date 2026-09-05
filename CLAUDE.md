# raymini

A small CPU raytracer, modernized from a 2013 student project (Qt/libQGLViewer
originally). Today: C++17, a GL-free core library, a GLFW + Dear ImGui viewer, a
headless CLI and a test suite. The goal is to iterate on rendering effects with
tests that prove each one.

## Layout

- `src/core/` — `raymini_core` static library. Vec3D, Vertex/Triangle/Mesh (OFF
  loader), BoundingBox, Ray (triangle + slab tests), Camera, Material, Light,
  Object, Scene, RayTracer, Image (stb). No GL, no GLFW: it links anywhere.
- `src/gui/Main.cpp` — `raymini`: GL 3.3 preview (left), raytraced panel
  (right), controls (bottom).
- `src/cli/Main.cpp` — `raymini-cli`: OFF in, PNG out. What the tests, CI and
  headless sessions use.
- `tests/` — `raymini_tests` (tiny harness in `tests/Test.h`, no external
  dependency) plus `tests/golden/*.png`.
- `third_party/` — Dear ImGui 1.91.5 (trimmed to core + GLFW/OpenGL3
  backends), glad, stb. `models/` — 26 OFF files.
- `claudedocs/EXPERIMENTS.md` — the next ten experiments, each with the test
  that proves it. `claudedocs/MODERNIZATION_ROADMAP.md` — the longer view.
- `Rendu.png` — reference render from the original project (ram on a ground
  plane with shadows). That look is the first target.

## Build, run, test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure

build/raymini models/minion.off                                   # viewer
build/raymini-cli teapot --mode normals --size 512x512 --yaw 25 --pitch 20 \
    --out renders/teapot.png                                      # headless
build/raymini-cli --help
```

- Options: `-DRAYMINI_BUILD_GUI=OFF` (no glfw/glm needed),
  `-DRAYMINI_BUILD_TESTS=OFF`. Executables land in `build/`.
- Dependencies: CMake >= 3.16, C++17 compiler. Viewer only: glfw3 + glm
  (`brew install glfw glm` / `apt install libglfw3-dev libglm-dev`).
- `renders/`, `build/` and `imgui.ini` are gitignored.
- CI (`.github/workflows/ci.yml`) builds and tests on Ubuntu and macOS and
  uploads sample renders as artifacts.

## Rendering pipeline as it exists today

- `Camera::primaryRay` -> `RayTracer::closestHit` (brute force over every
  triangle of every object, back faces culled) -> `RayTracer::shade` by mode.
- Modes: `lit` (Lambert: ambient + sum over lights of diffuse * max(0, n.l);
  no shadows, no specular), `ambient`, `hitmask`, `normals`, `depth`,
  `objectid`.
- Single-threaded. On an M-series Mac: teapot (880 triangles) at 256x256 in
  about 0.3 s; minion (84k triangles) at 160x160 in about 9 s.
- `Scene::addDefaultLights()` is the original cyan/yellow/white rig, scaled
  to the model's bounding box. Cyan light on the orange default material
  gives the green tint you see on renders; that is expected.
- Colors stay linear [0,1] until the final 8-bit conversion in `render()`.

## Conventions for adding an effect

1. Write the unit test first on synthetic geometry (`tests/Fixtures.h` has a
   quad and a cube); assert the physics (a shadowed pixel equals ambient, an
   edge pixel becomes gray with AA, ...).
2. Implement behind a `RayTracer` setter and a `raymini-cli` flag.
3. Regenerate goldens with `build/raymini_tests --filter golden --update-golden`
   and look at the PNG diff before committing them.
4. Render a PNG with `raymini-cli` and share it; the GUI needs a display.
5. Keep `src/core` free of GL/GLFW.

Golden comparison tolerates 4/255 per channel and 0.5 % of pixels differing
(silhouettes can flip across CPUs).

## Known gaps

- No acceleration structure: the old KdTree was removed (never built, unsafe
  to copy). A BVH is experiment 3.
- No shadows, specular, reflections, anti-aliasing, ambient occlusion,
  textures or threading yet. See `claudedocs/EXPERIMENTS.md`.
- Some OFF files are Z-up (teapot); the viewer and CLI assume Y-up.
- GL preview is 3:2, the raytraced panel is square; they share eye, target,
  up and vertical fov.
