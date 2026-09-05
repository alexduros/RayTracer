# Modernization Roadmap

## Where things stand (September 2026)

| Phase | Goal | Status |
|-------|------|--------|
| 1. Clean foundation | Qt removed, C++17, GLFW + OpenGL 3.3 core, Dear ImGui, stb image | done |
| 2. Dual rendering | GL preview + raytraced panel sharing one camera, headless CLI, tests, CI | done |
| 3. Rendering effects | shadows, specular, BVH, threads, AA, soft shadows, reflections, AO, tone mapping, DoF | next, see `EXPERIMENTS.md` |
| 4. Modern C++ pass | smart pointers where raw ownership remains, const-correctness, error handling | later |

## What phase 2 settled

- `raymini_core` has no GL/GLFW dependency; the GUI, the CLI and the tests
  all link the same library.
- Scene and RayTracer are plain values (no singletons). Scene loads nothing
  implicitly; `Scene::addObjectFromOFF` and `Scene::addDefaultLights` are
  explicit.
- `Camera` is the single source of truth for projection (vertical fov,
  aspect, top-left pixel origin) and mirrors `glm::lookAt` /
  `glm::perspective` in the viewer.
- The old KdTree, per-vertex ambient occlusion and anti-aliasing code were
  removed because they did not work; they come back as tested experiments.
- Every rendering change is guarded by unit tests on synthetic geometry and
  golden images of teapot and ram.

## Principles

1. Incremental: each experiment is one commit that keeps `ctest` green.
2. Test first: assert the physics on a quad or a cube before rendering a model.
3. Headless first: `raymini-cli` is the reference; the GUI just displays.
4. Keep the core portable; keep third-party code vendored and trimmed.
