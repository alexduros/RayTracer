# raymini

A small CPU raytracer, modernized from a 2013 student project (Qt/libQGLViewer
originally). Today: C++17, a GL-free core library, a GLFW + Dear ImGui viewer, a
headless CLI and a test suite. The goal is to iterate on rendering effects with
tests that prove each one.

## Layout

- `src/core/` — `raymini_core` static library. Vec3D, Vertex/Triangle/Mesh (OFF
  loader), ObjLoader (OBJ + MTL), BoundingBox, Ray (triangle + slab tests),
  Bvh, Camera, Material, Light, Object, Scene, RayTracer, Sampler, Optics (reflect,
  Snell, Fresnel), Image (stb). No GL, no
  GLFW: it links anywhere.
- `src/gui/Main.cpp` — `raymini`: GL 3.3 preview (left), raytraced panel
  (right), controls (bottom).
- `src/cli/Main.cpp` — `raymini-cli`: OFF in, PNG out. What the tests, CI and
  headless sessions use.
- `tests/` — `raymini_tests` (tiny harness in `tests/Test.h`, no external
  dependency) plus `tests/golden/*.png`.
- `third_party/` — Dear ImGui 1.91.5 (trimmed to core + GLFW/OpenGL3
  backends), glad, stb. `models/` — 26 OFF files plus `cube.obj`/`cube.mtl`
  (six materials, one per face) and `orientation.txt` (up axis per model).
- `claudedocs/RENDERING_ROADMAP.md` — every rendering mode the raytracer
  could offer next, one-sentence principle each; mirrored by
  `RayTracer::plannedMode()` (greyed out in the viewer's mode menu, printed
  by `--help`). Keep the three in sync. `claudedocs/EXPERIMENTS.md` — the
  implementation order, each step with the test that proves it.
  `claudedocs/MODERNIZATION_ROADMAP.md` — the longer view.
- `scripts/render-evolution.sh` renders `docs/evolution/*.png`, the README's
  step-by-step gallery (the ram, one picture per experiment, same camera);
  add a line per new visible effect and rerun it.
- `Rendu.png` — reference render from the original project (ram on a ground
  plane with shadows). That look is the first target.

## Build, run, test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure

build/raymini [models/minion.off]     # viewer; model optional, picker in Controls
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

- Formats: OFF (one object with the default material) and OBJ, optionally
  with MTL (one object per material, `Kd` -> colour, mean `Ks` -> specular;
  `vt` is parsed but dropped, `map_*` ignored). `Scene::addObjectsFromFile`
  dispatches on the extension, case-insensitively.
- `Camera::primaryRay` -> `RayTracer::closestHit` (each object's BVH, back
  faces culled) -> `RayTracer::shade` by mode.
- BVH (`src/core/Bvh.h`): median split, leaves of 4, a flat node array of
  indices, boxes padded by 1e-4 of the mesh's size. Built by
  `Object::update`, which must follow any in-place mesh edit. Ties at equal
  distance go to the lowest object, then triangle, index, so
  `setBvhEnabled(false)` / `--no-bvh` (brute force) gives identical hits;
  `tests/TestBvh.cpp` holds both paths to that, bit for bit.
- Modes: `lit` (ambient + per light: Lambert diffuse, Blinn-Phong highlight
  from Material::shininess, both scaled by the fraction of the light shadow
  rays find unblocked; `setShadows` / `setSpecularEnabled` switch the last two;
  ambient and diffuse scaled by the ambient occlusion when it is on;
  then on a material of transparency g, (1 - g) x that + g x clear glass
  (Fresnel share F of the mirror ray + 1 - F of the ray bent by Snell's
  law, `src/core/Optics.h`), and on a material of reflectivity k, (1 - k) x
  that + k x what the ray mirrored about the normal sees, recursively while
  depth < `setMaxDepth` (default 8, 0 = off bit for bit)),
  `ambient`, `hitmask`, `normals`, `depth`
  (z-buffer grey: white near, dark grey far, black = miss), `objectid`
  (golden-ratio hue palette by object index), `ao` (the open share of the
  hemisphere as grey; all white when occlusion is off). `RayTracer::info(mode)` holds
  each mode's name, principle, how to read it and its reference; the GUI
  shows it under the render, the CLI in `--help`, the README in a table.
  Keep the three in sync when adding a mode.
- Anti-aliasing: `RayTracer::setAntiAliasing(n, jitter)` traces n x n rays
  per pixel; jitter is seeded per pixel (`std::minstd_rand` raw output, no
  distribution) so it is reproducible and identical across tile orders,
  threads and platforms. Default 1 in the core and CLI (`--aa n --jitter`),
  2x2 in the GUI. Stats count rays (sub-samples), not pixels.
- Soft shadows: `RayTracer::setShadowSamples(n)` casts n x n shadow rays per
  light over a disk of `Light::radius` facing the point (jittered grid,
  concentric map); `lightVisibility` returns the unblocked fraction, the
  disk below the surface's horizon counting as hidden. n = 1 or radius 0 is
  the single hard-shadow ray, bit for bit. Default rig radius
  `Scene::kDefaultLightRadius` (0.1) x model size, `Scene::setLightRadius`;
  CLI `--shadow-samples n --light-radius f`, default 1 in the core and CLI,
  4x4 in the GUI (Soft shadows / Light size). Shadow rays ask
  `RayTracer::occluded` (stops at the first blocker).
- Randomness comes from `Sampler` (`src/core/Sampler.h`): seeded per pixel
  and per stream (AA jitter, shadows, occlusion), so pictures do not depend
  on tile order and one effect never reshuffles another's samples. The
  samplers `shade` needs travel together as `PixelSamplers`; add a stream
  and a member there for each new sampled effect.
- Ambient occlusion: `RayTracer::setAmbientOcclusion(n, radius)` casts n x n
  cosine-weighted hemisphere rays (jittered grid on the concentric disk,
  lifted), open if they meet nothing within the radius; 0 = off (default).
  CLI `--ao n --ao-radius f` (fraction of the model size, 0.2), GUI
  Occlusion / AO radius. Costs n x n rays per hit: ram at 384x256 0.02 s ->
  0.24 s with 8x8 on one thread.
- `RenderJob` traces tile by tile (32 px) on N worker threads (default one
  per core, `RenderJob::defaultThreadCount`, never more than the tiles) that
  pull tile indices from an atomic counter, top row first; each worker keeps
  its tile's stats private and publishes the tile and its stats under a
  mutex. The GUI shows the partial image with a progress bar and can cancel
  (Threads slider in Render), the CLI prints a percentage on a terminal
  (`--threads n`, 0 = auto). `RayTracer::render()` is the synchronous,
  single-threaded reference and tests assert the job's pixels and statistics
  are byte-identical for any tile size and thread count; `renderRegion` is
  const and the scene read-only, so workers share them (TSan-clean). On an
  M-series Mac (10 cores), teapot at 256x256 renders in about 5 ms on one
  thread (0.29 s brute force) and minion (84k triangles) on its ground with
  shadows in about 20 ms (60 s brute force); loading the OFF file (0.1 s)
  now dominates. Soft shadows multiply the shadow work by n x n: ram on its
  ground at 384x384 with 2x2 AA and 8x8 shadow rays takes 3.9 s on one
  thread, 0.75 s on ten.
- `Scene::addDefaultLights()` is the original cyan/yellow/white rig, scaled
  to the model's bounding box. Cyan light on the orange default material
  gives the green tint you see on renders; that is expected.
- Mirror reflections: `Material::reflectivity` (0 by default, so nothing
  changes unless asked), per-object overrides `Scene::setModelReflectivity` /
  `setGroundReflectivity`; CLI `--reflectivity k --ground-reflectivity k
  --max-depth n`, GUI mirror slider next to Ground, Mirror, Bounces. Reflected
  rays reuse the pixel's shadow sampler and are not counted in `Stats`.
- Glass: `Material::transparency` / `ior` (MTL `d`, `Tr`, `Ni`), CLI
  `--transparency g --ior n` (only when given), GUI Glass / Index. Rays
  inside an object are two-sided (`Ray(o, d, true)`: `Ray::hit` skips back-face
  culling), and `Hit::backFace` (from the geometric normal) says whether a
  hit leaves the object, which orders the indices. `Hit::triangleIndex` comes
  from `nearestHit`. Every other ray still culls back faces. Glass shadows
  are opaque; a glass hit splits a ray in two, so depth costs.
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

- One BVH per object, no tree over the objects: `closestHit` still loops
  over every object. Fine for a model, its ground plane and an OBJ's few
  material groups; a scene of many objects would want a top-level BVH.
- `Scene::addGroundPlane()` adds a *backdrop* quad at the bottom of the
  model's box; backdrops are skipped by `updateBoundingBox`, so framing,
  depth defaults and the light rig keep following the model. CLI
  `--ground`, GUI "Ground" checkbox (on by default).
- No textures yet; glass is clear and casts opaque shadows (no caustics). See `claudedocs/EXPERIMENTS.md`. MTL `map_Kd` textures are ignored;
  OBJ texture coordinates are parsed but not stored.
- Up axis: the scene is Y-up and `Scene::setUpAxis` rotates a model on
  load (exact axis permutation) so its own up axis becomes +Y; call it
  before `addDefaultLights`. `Orientation.h` resolves "Auto" from
  `models/orientation.txt` (curated for every bundled model) and otherwise
  from the flattest-side heuristic, which is wrong for models with a flat
  back (the minion), hence the manifest and the `--up` / "Up axis"
  override. Add new bundled models to the manifest.
- GL preview and raytraced panel share eye, target, up, vertical fov and
  aspect (3:2), so a render reproduces the preview exactly.
