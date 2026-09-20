# Changelog

Every release is one step of `claudedocs/RAY_TRACING_TIMELINE.md`: a landmark
ray tracing paper turned into code, with the test that proves it. Format
follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions
follow [semantic versioning](https://semver.org/spec/v2.0.0.html), where a
new rendering feature bumps the minor.

The release workflow reads the section of the version it is tagging and
publishes it as the release notes, with the binaries for macOS and Linux.

## [Unreleased]

## [0.4.0] - 2026-09-20

### Added

- **Spot**, Keenan Crane's cow (CC0): 5 856 triangles with texture
  coordinates and a texture map, the only model here that has them — OFF
  stores no UVs. What texture mapping will be built on (timeline step 12).
- **Belly**, the project's mascot: 74 478 triangles over five materials,
  also with texture coordinates.
- `models/README.md`: what each model is for and where it comes from.

### Removed

- Two dozen course models nothing tested or documented (the minion, the
  dragon, the shuttle, the Klingon ship...): 7 MB out of the repository and
  out of every release archive. Six shapes remain, each earning its place.
  Any OFF or OBJ file still loads by path.

### Changed

- The BVH's heavy-model test moved from the minion to `ram_HD` (50 544
  triangles): same hits as brute force on 1 000 random rays, 0.018 s against
  40.7 s at 256x256 with shadows, on one thread.
- The OFF loader's face-colour test reads the ram rather than the seashell.

## [0.3.0] - 2026-09-20

### Changed

- **The default ambient light drops from 0.15 to 0.05.** The old value was
  chosen when radiance went straight into bytes, a conversion that darkens
  the mid-tones and passed for contrast. Since 0.2.0 the display lifts them
  properly (sRGB), and 0.15 washed the shadows out. Every lit picture gains
  contrast; the twelve lit goldens and the gallery are regenerated, the
  other modes are untouched, and `--ambient` still overrides it.

## [0.2.0] - 2026-09-20

Step 11 of the timeline: R. A. Goldstein & R. Nagel, "3-D Visual
Simulation", *Simulation* 16(1), 1971 — the first useful ray caster, where
solids were equations, not triangles.

### Added

- **Analytic primitives** (`src/core/Primitive.h`): a sphere, a cylinder and
  a disc, met where a polynomial vanishes. One root instead of thousands of
  triangle tests, and a silhouette that stays exact at any zoom. An `Object`
  now holds a mesh or a primitive; the tracer intersects both, and a
  primitive respects two-sided rays, so it can be glass.
- **`--sphere <x> <y> <z> <r> <matte|mirror|glass>`** in `raymini-cli`,
  repeatable, placed in half model sizes around the model's centre.
- Golden `spheres_lit` (three spheres on their ground, no model file) and a
  gallery picture of the ram between a mirror sphere and a glass one.

### Tests

- A sphere is hit at the root of its quadratic, from 200 directions, with
  the outward unit normal; it is missed when the discriminant says so, and
  grazed on the tangent.
- From inside, only a two-sided ray meets it, and the hit is reported as a
  back face: what glass needs.
- The silhouette matches the analytic disc to the pixel, at 128x128.
- A tessellated sphere converges to it: the error falls by more than half
  every doubling, stays under the sagitta of the facets it is made of, and
  never reaches zero.

## [0.1.0] - 2026-09-20

The first packaged release: everything the first nine experiments built,
after thirteen years (a 2013 student project on Tamy Boubekeur's raymini
framework, modernised to C++17, GLFW and Dear ImGui).

### Added

- **Renderer.** Ray/triangle intersection through a bounding volume hierarchy
  per object (Kay & Kajiya 1986), traced tile by tile on every core, with a
  synchronous single-threaded reference the tests compare against.
- **Shading.** Lambert diffuse and the Blinn-Phong highlight (Phong 1975,
  Blinn 1977), hard and soft shadows from disk lights (Appel 1968; Cook,
  Porter & Carpenter 1984), ambient occlusion (Zhukov, Iones & Kronin 1998),
  mirror reflections and glass with Snell's law and the Fresnel equations
  (Whitted 1980), an optional ground plane that catches the shadows.
- **Image.** n x n supersampling with optional jitter (Whitted 1980, Cook
  1986), a floating-point radiance buffer, and a display that meters the
  exposure, applies a tone curve (Reinhard 2002, or ACES) and encodes in
  sRGB; `.hdr` output keeps the radiance (Ward 1991).
- **Modes.** Lit, ambient, hit mask, normals, depth, object id and ambient
  occlusion; each explains what it computes, how to read it and where it
  comes from, in the viewer, in `--help` and in the README.
- **Formats.** OFF, and OBJ with its MTL materials (colour, specular,
  shininess, transparency, index of refraction). Models are stood upright on
  load.
- **Tools.** `raymini-cli`, a headless renderer (`--version`, and a flag per
  effect), and `raymini`, a GLFW + Dear ImGui viewer with a GL preview, a
  raytraced panel that fills in tile by tile, and a display row that re-maps
  the last render without tracing it again.
- **Tests.** 114 cases and 7903 checks on synthetic geometry, each proving a
  physical property or an invariant, plus 42 golden images, run on Ubuntu and
  macOS by the CI.

[Unreleased]: https://github.com/alexduros/RayTracer/compare/v0.4.0...HEAD
[0.4.0]: https://github.com/alexduros/RayTracer/releases/tag/v0.4.0
[0.3.0]: https://github.com/alexduros/RayTracer/releases/tag/v0.3.0
[0.2.0]: https://github.com/alexduros/RayTracer/releases/tag/v0.2.0
[0.1.0]: https://github.com/alexduros/RayTracer/releases/tag/v0.1.0
