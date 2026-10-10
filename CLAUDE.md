# raymini

A small CPU raytracer, modernized from a 2013 student project (Qt/libQGLViewer
originally). Today: C++17, a GL-free core library, a GLFW + Dear ImGui viewer, a
headless CLI and a test suite. The goal is to iterate on rendering effects with
tests that prove each one.

## Layout

- `src/core/` — `raymini_core` static library. Vec3D, Vertex/Triangle/Mesh (OFF
  loader), ObjLoader (OBJ + MTL), BoundingBox, Ray (triangle + slab tests),
  Bvh, Camera, Material, Light, Object, Scene, RayTracer, Sampler, Optics (reflect,
  Snell, Fresnel), Primitive (sphere, cylinder, disc), Texture, Bump (the
  perturbed normal), Environment (the panorama around the scene), Image
  (stb). No GL, no GLFW: it links
  anywhere.
- `src/gui/Main.cpp` — `raymini`: GL 3.3 preview (left), raytraced panel
  (right), controls (bottom).
- `src/cli/Main.cpp` — `raymini-cli`: OFF in, PNG out. What the tests, CI and
  headless sessions use.
- `tests/` — `raymini_tests` (tiny harness in `tests/Test.h`, no external
  dependency) plus `tests/golden/*.png`.
- `third_party/` — Dear ImGui 1.91.5 (trimmed to core + GLFW/OpenGL3
  backends), glad, stb. `models/` — six shapes and what each is for, listed
  in `models/README.md`: `teapot.off`, `ram.off`, `ram_HD.off` (50k
  triangles, the heavy one), `cube.obj`/`cube.mtl` (six materials),
  `spot.obj` + `spot_texture.png` (the only UVs, CC0, for texture mapping)
  and `belly.obj`/`belly.mtl` (the mascot, five materials, 74k triangles).
  `orientation.txt` gives the up axis of each. `venice_sunset.hdr` (CC0,
  1024x512) is the one environment map, `dimples.png` the one height map
  (written by `scripts/make-dimples.py`; 8-bit grey, so `Image::load`
  expands grey to RGB); `scripts/package.sh` ships every `*.png` and
  `*.hdr` beside the meshes.
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

build/raymini [models/belly.obj]      # viewer; model optional, picker in Controls
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
  with MTL (one object per material, `Kd` -> colour, mean `Ks` -> specular,
  `map_Kd` -> texture, `map_bump` / `bump` -> height map with `-bm` as its
  scale; `vt` kept). `Scene::addObjectsFromFile`
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
  `ambient`, `hitmask`, `normals` (the shading normal, so a bump map
  shows), `depth`
  (z-buffer grey: white near, dark grey far, black = miss), `objectid`
  (golden-ratio hue palette by object index), `ao` (the open share of the
  hemisphere as grey; all white when occlusion is off), `uv` (texture
  coordinates, u to red and v to green; black without any). `RayTracer::info(mode)` holds
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
  thread (0.29 s brute force) and ram_HD (50k triangles) on its ground with
  shadows in 18 ms (41 s brute force); loading the file now dominates. The
  same held for the minion (84k triangles, 17 ms against 60 s) while it was
  bundled. Soft shadows multiply the shadow work by n x n: ram on its
  ground at 384x384 with 2x2 AA and 8x8 shadow rays takes 3.9 s on one
  thread, 0.75 s on ten.
- Ambient light is 0.05 by default (`setAmbientIntensity`, CLI `--ambient`).
  It was 0.15 while radiance went straight into bytes; the display of
  experiment 9 lifts the dark tones, so that much ambient flattened every
  picture.
- `Scene::addDefaultLights()` is the original cyan/yellow/white rig, scaled
  to the model's bounding box. Cyan light on the orange default material
  gives the green tint you see on renders; that is expected.
- Mirror reflections: `Material::reflectivity` (0 by default, so nothing
  changes unless asked), per-object overrides `Scene::setModelReflectivity` /
  `setGroundReflectivity`; CLI `--reflectivity k --ground-reflectivity k
  --max-depth n`, GUI mirror slider next to Ground, Mirror, Bounces. Reflected
  rays reuse the pixel's shadow sampler and are not counted in `Stats`.
- Textures (`src/core/Texture.h`, experiment 12): an OBJ's `vt` are kept
  (`Vertex::getU/getV`, de-duplicated on position + coordinate + normal, so
  a seam stays two vertices), interpolated in `Ray::hit` like the normal,
  and read bilinearly with wrapping; texels are decoded from sRGB to linear
  at load. `Material::getColorAt(u, v)` is the colour to shade with — never
  `getColor()` in the shading path. MTL `map_Kd`, CLI `--texture`, mode
  `uv`. Only OBJ carries coordinates; OFF models read (0, 0) everywhere.
- Bump mapping (`src/core/Bump.h`, step 14): `Material::setBumpMap` holds
  a height map (a `Texture` read with `Texture::readData`: byte / 255, no
  sRGB decode) and `setBumpScale` the world height of white above black.
  `RayTracer::shadingNormal(scene, hit)` returns the interpolated normal
  tilted by Blinn's formula (`bump::perturb`), with Pu, Pv from the hit
  triangle's corners (`bump::tangents`, false without coordinates) and the
  slopes from `Texture::heightGradient` (neighbour differences, bilinear,
  exactly 0 on a flat map). Lit uses it for Lambert, the highlight and the
  mirror / glass directions; `directLight` takes both normals, because
  shadow and occlusion rays, offsets and the back-face flip keep the
  surface's own; a mirrored or bent ray on the wrong side of the surface is
  folded about it, only when bumped. No map, a flat map, scale 0 or
  `setBumpMapping(false)` are bit for bit the smooth picture. Meshes with
  UVs only (OBJ): primitives and the ground plane carry none. CLI `--bump
  <file>` (bare name in `models/`) `--bump-scale f` (fraction of the model
  size, 0.01) `--no-bump`, GUI Bumps / Height.
- Environment map (`src/core/Environment.h`, step 13): `Scene::setEnvironment`
  holds a latitude-longitude panorama (shared, immutable), and
  `RayTracer::escaped` returns it for every ray that meets nothing in Lit
  mode, primary or bounced; the analysis modes and a scene without one keep
  `backgroundColor`, so nothing else moved. `Environment::toMap`: u = 1/2 +
  atan2(x, -z) / 2pi across (the middle is -Z, where a camera at yaw 0
  looks; +X is to its right), v = acos(y) / pi down from the top row.
  `sample` is `Texture::sample` with v held half a texel inside, so
  longitude wraps and the poles do not mix. A Radiance `.hdr` loads as
  linear floats (`HdrImage::isRadiance`, also for `Texture::load`), anything
  else through the sRGB decode. It is seen, not a light: lighting by the
  map is step 46. CLI `--environment <file>` (a bare name resolves in
  `models/`), GUI World picker over `models/*.hdr`.
- The display (filmic) applies to Lit only: the analysis modes show values,
  not light, so the CLI and the viewer render them through
  `Display::linear()`.
- Analytic primitives (`src/core/Primitive.h`, experiment 11): an `Object`
  holds a mesh or a `Primitive` (shared, immutable), and `closestHit` /
  `occluded` intersect whichever it is; a primitive has no BVH (one root)
  and its normal is geometric, so `Hit::backFace` comes straight from it.
  They live in scene coordinates: `Scene::setUpAxis` leaves them alone, so
  add them after orienting the model. CLI `--sphere x y z r
  matte|mirror|glass`; the CLI applies the model's options (`--reflectivity`,
  `--transparency`, `--color`, `--texture`...) before the spheres join the
  scene, so they keep their own material (they did not until v0.6.0).
- Glass: `Material::transparency` / `ior` (MTL `d`, `Tr`, `Ni`), CLI
  `--transparency g --ior n` (only when given), GUI Glass / Index. Rays
  inside an object are two-sided (`Ray(o, d, true)`: `Ray::hit` skips back-face
  culling), and `Hit::backFace` (from the geometric normal) says whether a
  hit leaves the object, which orders the indices. `Hit::triangleIndex` comes
  from `nearestHit`. Every other ray still culls back faces. A glass hit
  splits a ray in two, so depth costs.
- Tinted glass and glass shadows (step 15): `Material::setAbsorption` is a
  colour per world unit (`Material::absorptionFor(tint, depth)`, MTL `Tf`
  at depth 1), 0 = clear. `RayTracer::bounce` takes the `medium` a
  secondary ray travels in (the hit's material when the ray stays or goes
  inside) and the surface point it left, and multiplies what the ray brings
  back by `optics::transmittance` over the distance to its hit; a ray that
  escapes (open mesh) is not charged, and clear glass skips the product, so
  its floats did not move. Shadow rays ask `RayTracer::transmission`
  (a Vec3Df): opaque objects first (`blocked(..., opaqueOnly)`, exactly
  the old `occluded`), then the transparent ones face by face with a
  two-sided ray (`nearest(..., transparentOnly)`): transparency x (1 - F)
  per face, F taken on the air side, and the absorption between a front
  face and the back face that follows. Not bent, no caustics.
  `lightVisibility` therefore returns a Vec3Df, and `directLight` applies
  it per channel in the same order of products as before, so pictures
  without glass are bit for bit the old ones. `setTransparentShadows(false)`
  / `--opaque-shadows` is the old all-or-nothing; the gallery's pictures
  before step 15 use it. CLI `--tint r g b` (implies `--transparency 1`)
  `--tint-depth f` (fraction of the model size, 0.25), GUI Tint / Tint
  depth / Through glass. Costs: the glass ram on its floor at 384x256 with
  2x2 AA, 8x8 shadow rays and 8x8 occlusion takes 4 s on ten threads,
  against 1.8 s with opaque shadows.
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
- Glass focuses no light (no caustics, timeline step 27): its shadow is
  filtered along straight rays. Absorption is charged per stretch that ends
  on a face, so open meshes and glass inside glass are approximate.
  Textures are read bilinearly with no filtering at a distance (step 19).
  The environment map is seen but lights nothing (step 46), and the GL
  preview does not show it. Bump maps tilt normals only (smooth
  silhouettes, no shadows between bumps), need an OBJ's coordinates, and
  are read unfiltered like textures.
- Up axis: the scene is Y-up and `Scene::setUpAxis` rotates a model on
  load (exact axis permutation) so its own up axis becomes +Y; call it
  before `addDefaultLights`. `Orientation.h` resolves "Auto" from
  `models/orientation.txt` (curated for every bundled model) and otherwise
  from the flattest-side heuristic, which is wrong for models with a flat
  back (it lays Spot on her side), hence the manifest and the `--up` /
  "Up axis" override. Add new bundled models to the manifest.
- GL preview and raytraced panel share eye, target, up, vertical fov and
  aspect (3:2), so a render reproduces the preview exactly.
