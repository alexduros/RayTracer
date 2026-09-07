# raymini

A small CPU raytracer with a real-time OpenGL preview, a headless renderer and
a regression test suite.

Originally written in 2013 by Benjamin Combourieu, Alexandre Duros and Raphaël
Moutard on top of Tamy Boubekeur's raymini teaching framework (Qt +
libQGLViewer). Modernized to C++17, GLFW, OpenGL 3.3 core, Dear ImGui and stb,
with the raytracer split into a library that builds without any GL dependency.

![Reference render from the original project](Rendu.png)

## What it does today

- Loads OFF (colour columns and comments tolerated) and OBJ meshes. An OBJ
  may reference an MTL file: each material becomes its own object (`Kd`
  colour, `Ks` specular, `Ns` shininess); texture maps are not used yet.
  Polygons are fan-triangulated; normals come from the file or are
  recomputed. Models are stood upright on load (see Notes).
- Viewer: orbit and zoom the mesh in a GL 3.3 preview, render the same camera
  with the raytracer, save the result as PNG.
- Raytracer: ray/triangle intersection (brute force, back faces culled),
  Lambert + Blinn-Phong shading with point lights and hard shadows, an
  optional ground plane that catches them, n x n supersampling with
  optional jitter, and analysis modes (hit mask, normals, depth, object id).
  Every mode explains itself in the UI and in `--help`, with the study it
  comes from; see "Render modes" below.
- CLI: render any model to a PNG, no display needed.
- Tests: unit tests on synthetic geometry and golden-image regression on the
  teapot and ram models. CI runs them on Ubuntu and macOS.

## Build

Requirements: CMake 3.16+, a C++17 compiler; for the viewer, GLFW 3 and GLM.

```bash
# macOS
brew install cmake glfw glm
# Ubuntu / Debian
sudo apt-get install build-essential cmake libglfw3-dev libglm-dev libgl1-mesa-dev

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

`-DRAYMINI_BUILD_GUI=OFF` skips the viewer (no GLFW/GLM needed);
`-DRAYMINI_BUILD_TESTS=OFF` skips the tests. Executables are written to `build/`.

## Run

```bash
build/raymini [models/minion.off]                     # viewer (model optional)
build/raymini-cli teapot --mode normals --size 512x512 --yaw 25 --pitch 20
build/raymini-cli cube --mode lit --yaw 25 --pitch 20  # OBJ + MTL sample
build/raymini-cli ram --ground --aa 2 --yaw -35 --pitch 15   # floor + shadows, the Rendu.png look
build/raymini-cli teapot --up +y                       # override the file's up axis (auto: orientation.txt)
build/raymini-cli --help                              # all options
```

The CLI writes `renders/<model>_<mode>.png` by default; bare model names
resolve to `models/`.

Viewer controls:

- The layout follows the window: resize it or go full screen and the panels
  and the preview scale with it (the preview is re-rendered at the displayed
  size, so it stays sharp).
- Controls panel, four sections: Model (picker over every `.off` and `.obj`
  in `models/`, mesh stats), Camera (FOV, position, target, Reset), Preview
  (wireframe, back-face culling), Render (output width, mode: Lit, Ambient,
  Hit mask, Normals, Depth, Object id; depth range in Depth mode).
- Left-drag in the preview to orbit, scroll to zoom.
- Raytracer panel: Render Scene traces on a worker thread, so the UI stays
  live while the image fills in tile by tile behind a progress bar; Cancel
  stops it. Then Save PNG (into `renders/`), timing and hit ratio. Starts on
  the teapot unless a path is given on the command line.

## Render modes

The same text is shown under the render in the viewer and printed by
`raymini-cli --help`; it lives in `RayTracer::info()`.

| Mode | What it computes | How to read it | Study |
|------|------------------|----------------|-------|
| **Lit (Lambert + Blinn-Phong, shadows)** | Per light: material colour × light colour × max(0, n·l) (Lambert), a white highlight where the half-vector between light and view aligns with the normal, raised to the shininess (Blinn-Phong), and a shadow ray toward the light that drops it when blocked; plus a constant ambient term. No bounces yet. | Brighter where a surface faces a light; tight bright spots are highlights; blocked lights leave only the ambient term. Colour is material × light, so the cyan key light tints the orange default material green. Turn on the ground plane to see shadows fall. | J. H. Lambert, *Photometria* (1760); J. Blinn, "Models of Light Reflection for Computer Synthesized Pictures", SIGGRAPH 1977; shadow rays: A. Appel, AFIPS 1968, T. Whitted, CACM 23(6), 1980 |
| **Ambient (albedo)** | The material's base colour (Kd) at the hit, unlit. | Flat silhouettes per material; checks materials and outlines, shows no shape. | Ambient term of B. T. Phong, "Illumination for Computer Generated Pictures", CACM 18(6), 1975 |
| **Hit mask (coverage)** | White where the primary ray hits geometry, black where it escapes. | A binary silhouette; with anti-aliasing, edge pixels turn grey in proportion to coverage. | T. Porter & T. Duff, "Compositing Digital Images", SIGGRAPH 1984 |
| **Normals** | Surface normal remapped from [-1, 1] to [0, 1]: x→red, y→green, z→blue. | A face pointing at the camera is light violet, one pointing up light green; flat patches are hard edges. | Normal-map encoding: Cohen, Olano & Manocha, "Appearance-Preserving Simplification", SIGGRAPH 1998; Blinn, "Simulation of Wrinkled Surfaces", SIGGRAPH 1978 |
| **Depth** | Eye-to-hit distance mapped between near and far: white at near, dark grey at far, black = nothing hit. | Brighter is closer; tighten near/far around the model if it is all one shade. | The z-buffer: E. Catmull, PhD thesis, University of Utah, 1974 |
| **Object id** | One palette colour per object (per material group for OBJ). | Same colour = same object; a one-colour OFF model is expected. | The item buffer: Weghorst, Hooper & Greenberg, "Improved Computational Methods for Ray Tracing", ACM TOG 3(1), 1984 |

**Anti-aliasing** (`--aa n`, `--jitter`; Anti-alias / Jitter in the viewer):
n × n primary rays per pixel averaged in linear colour. Jitter offsets each
ray inside its cell with a per-pixel seed, so renders stay reproducible and
independent of tile order. One ray per pixel gives staircase edges; 2x2 or
3x3 smooths them at 4x or 9x the cost. Whitted, "An Improved Illumination
Model for Shaded Display", CACM 23(6), 1980; Cook, "Stochastic Sampling in
Computer Graphics", ACM TOG 5(1), 1986.

## Test

```bash
ctest --test-dir build --output-on-failure
build/raymini_tests --filter camera                    # a subset
build/raymini_tests --filter golden --update-golden    # after an intentional rendering change
```

Golden images live in `tests/golden/`; review their diff before committing a
regeneration.

## Layout

```
CMakeLists.txt          root project; options RAYMINI_BUILD_GUI / RAYMINI_BUILD_TESTS
src/core/               raymini_core: mesh, ray, camera, scene, raytracer, image
src/gui/Main.cpp        raymini (GLFW + Dear ImGui viewer)
src/cli/Main.cpp        raymini-cli (headless renderer)
tests/                  raymini_tests + tests/golden/*.png
third_party/            imgui, glad, stb
models/                 OFF models (teapot, ram, minion, dragon, ...), cube.obj/.mtl, orientation.txt
claudedocs/             EXPERIMENTS.md (next steps), MODERNIZATION_ROADMAP.md
.github/workflows/      CI: build + tests + sample renders on Ubuntu and macOS
```

## Next steps

`claudedocs/RENDERING_ROADMAP.md` maps the rendering modes the raytracer can
offer next (shading, light transport, textures, camera effects, analysis
modes, and what complex scenes need), each with a one-sentence principle;
the viewer lists them greyed out under "Planned" in the mode menu and
`raymini-cli --help` prints them. `claudedocs/EXPERIMENTS.md` is the
implementation order, each step with the test that proves it.

## Notes

- The scene is Y-up, but model files follow no convention (the teapot and
  the ram are Z-up, the minion Y-up). Each model is rotated on load so its
  own up axis becomes +Y: `models/orientation.txt` lists the axis for every
  bundled model, files not listed get a heuristic (the flattest side of the
  bounding box is the bottom), and the viewer's "Up axis" menu or
  `raymini-cli --up` overrides both. Add a line to `orientation.txt` next to
  your own models to make them load upright.
- The default lights are the original project's cyan / yellow / white rig,
  scaled to the model and kept above its base.
