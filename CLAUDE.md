# raymini

A small CPU raytracer, modernized from a 2013 student project (Qt/libQGLViewer
originally). Today: C++17, a GL-free core library, a GLFW + Dear ImGui viewer, a
headless CLI and a test suite. The goal is to iterate on rendering effects with
tests that prove each one.

## Layout

- `src/core/` — `raymini_core` static library. Vec3D, Vertex/Triangle/Mesh (OFF
  loader), ObjLoader (OBJ + MTL), BoundingBox, Ray (triangle + slab tests),
  Bvh, Camera, Material, Light, Object, Scene, RayTracer, Sampler, Optics (reflect,
  Snell, Fresnel), Primitive (sphere, cylinder, disc), Image (stb). No GL, no
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
  `claudedocs/RAY_TRACING_TIMELINE.md` — the plan after experiment 10: one
  step per landmark ray tracing paper, 1971 to 2026, in chronological order
  (steps 11 to 94, the backbone marked ★).
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
- Debugging: breakpoints need a Debug tree, `cmake -S . -B build-debug
  -DCMAKE_BUILD_TYPE=Debug && cmake --build build-debug -j`, which is what
  the VS Code launch configurations run and debug. In `build/` (Release, no
  `-g`) a breakpoint on the tracing hot path reports "no locations" and never
  fires, because it is inlined away. That hot path lives in headers — the
  slab test `Ray::intersect(box, invDirection, tMax, tEntry)` in `Ray.h`, the
  traversal in `Bvh.cpp` — so set the breakpoint there; the `intersect`
  overload in `Ray.cpp` is only ever called by the tests. One ray under a
  debugger beats a whole frame: `raymini_tests --filter bvh:` or
  `raymini-cli teapot --size 8x8 --threads 1`.

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
- Analytic primitives (`src/core/Primitive.h`, experiment 11): an `Object`
  holds a mesh or a `Primitive` (shared, immutable), and `closestHit` /
  `occluded` intersect whichever it is; a primitive has no BVH (one root)
  and its normal is geometric, so `Hit::backFace` comes straight from it.
  They live in scene coordinates: `Scene::setUpAxis` leaves them alone, so
  add them after orienting the model. CLI `--sphere x y z r
  matte|mirror|glass`.
- Glass: `Material::transparency` / `ior` (MTL `d`, `Tr`, `Ni`), CLI
  `--transparency g --ior n` (only when given), GUI Glass / Index. Rays
  inside an object are two-sided (`Ray(o, d, true)`: `Ray::hit` skips back-face
  culling), and `Hit::backFace` (from the geometric normal) says whether a
  hit leaves the object, which orders the indices. `Hit::triangleIndex` comes
  from `nearestHit`. Every other ray still culls back faces. Glass shadows
  are opaque; a glass hit splits a ray in two, so depth costs.
- Display (experiment 9): the tracer writes linear radiance, above 1 kept,
  into an `HdrImage` (`renderHdr`, `renderRegion`, `RenderJob::hdrSnapshot`);
  a `Display` (`src/core/Display.h`) maps it to bytes last: exposure in stops
  (set, or metered to the key 0.18 then corrected), a curve (none, Reinhard,
  ACES), an encoding (linear, sRGB, gamma). The tracer's default is
  `Display::linear ()`, the historical bytes, so unit tests and goldens keep
  their values; the CLI and the viewer default to `Display::filmic ()`
  (metered, ACES, sRGB), `--display linear` for the old look, `.hdr` output
  for the radiance. The viewer's display row re-maps the last render without
  tracing. CLI rig and material overrides: `--ambient`, `--light`,
  `--color`, `--specular`, `--shininess`.

## Shipping a step

Each step of the timeline is a release. The loop, from the paper to the tag:

1. Send the paper's link, so it can be read while the work starts.
2. Branch (`step-<n>-<slug>`), build the step the way the section below
   says, open a pull request. CI builds and tests it on Ubuntu and macOS and
   attaches the release archive, so packaging is proven before the tag.
3. Add the entry to `CHANGELOG.md` under a new version heading (a rendering
   feature bumps the minor) and bump `project(raymini VERSION ...)` in
   `CMakeLists.txt`; `raymini-cli --version` and the ctest `cli_version`
   follow from it.
4. Merge the pull request, then tag it: `git tag v<version> && git push
   origin v<version>`. `.github/workflows/release.yml` refuses a tag that
   disagrees with `CMakeLists.txt`, builds both platforms, runs the tests,
   packages with `scripts/package.sh` and publishes the GitHub release with
   the changelog section as its notes.

`scripts/package.sh build dist` runs locally and writes the same archive as
CI: the binaries, `models/`, the README, the changelog and a RUNNING.txt.
`scripts/changelog-section.sh <version>` prints the notes the release will
carry.

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
