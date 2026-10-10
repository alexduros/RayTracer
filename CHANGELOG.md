# Changelog

Every release is one step of `claudedocs/RAY_TRACING_TIMELINE.md`: a landmark
ray tracing paper turned into code, with the test that proves it. Format
follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions
follow [semantic versioning](https://semver.org/spec/v2.0.0.html), where a
new rendering feature bumps the minor.

The release workflow reads the section of the version it is tagging and
publishes it as the release notes, with the binaries for macOS and Linux.

## [Unreleased]

## [0.8.0] - 2026-10-10

Step 15 of the timeline: D. S. Kay & D. Greenberg, "Transparency for
Computer Synthesized Images", SIGGRAPH 1979 — what a transparent object
lets through falls off with the thickness the light crosses in it.

### Added

- **Tinted glass.** `--tint <r> <g> <b>` gives the model's glass a colour:
  what white light keeps after crossing `--tint-depth` of it (a quarter of
  the model's size by default). Inside the glass a ray loses a share of its
  light per unit of length, a different share per channel, exp(-sigma x d)
  after a distance d (Beer-Lambert), on every leg of its path, the ones
  mirrored inside included. The colour therefore comes from the thickness:
  a body is deep, a handle or a rim pale, and twice the glass passes the
  square of the share. `Material::setAbsorption`, MTL `Tf` (what one unit
  of the model's length lets through), Tint and Tint depth in the viewer.
  Clear glass is untouched, to the last bit.
- **`--tint` alone makes the model glass**: `raymini-cli teapot --ground
  --tint 0.95 0.55 0.1` is an amber teapot.

### Changed

- **Glass no longer casts a black shadow.** A shadow ray that meets a
  transparent material is filtered instead of stopped: at each face it
  keeps the share the Fresnel reflection leaves, 0.96 head-on for an index
  of 1.5, less at a grazing angle, and between the face it enters by and
  the one it leaves by, what the tint absorbs. Clear glass now casts a pale
  shadow with a darker outline, tinted glass a coloured one, and a pane of
  index 1 none at all. Shadow rays are not bent, so no bright spot gathers
  under a curved body: caustics are not traced. This is our extension of
  the paper's idea to the light itself, and it changes every picture with
  glass above a surface: `--opaque-shadows` gives the old ones back, byte
  for byte (`RayTracer::setTransparentShadows`, Through glass in the
  viewer). A scene without glass is unchanged either way.
- Goldens `teapot_ground_glass_lit` and `spheres_lit` regenerated for those
  shadows; new golden `teapot_ground_amber_lit`; ctest `cli_tint`,
  `cli_opaque_shadows`, `cli_bad_tint`.

Not yet: glass with holes in it (the teapot's lid sits on an open body) is
absorbed only over the stretches that end on a face, and one glass object
inside another is charged the outer one's absorption on the way in only.

## [0.7.0] - 2026-10-05

Step 14 of the timeline: J. F. Blinn, "Simulation of Wrinkled Surfaces",
SIGGRAPH 1978 — a height map does not move the surface, it tilts the normal
the light is computed with, and the eye reads the shading as relief.

### Added

- **Bump mapping.** `--bump <file>` reads a grey picture through the model's
  texture coordinates as a height, white above black, and shades the model
  as if its surface had been pushed out by that much: Blinn's perturbed
  normal, N' = N + (Fu (N x Pv) - Fv (N x Pu)) / |N| (`src/core/Bump.h`),
  with the tangents Pu, Pv taken from each triangle's corners and the slopes
  Fu, Fv from neighbouring texels. Lambert, the highlight, mirror and glass
  rays all use the tilted normal; shadow and occlusion rays still leave the
  real surface. `--bump-scale <f>` sets how high white stands, in model
  sizes (0.01 by default, negative digs), `--no-bump` ignores every map,
  and `--mode normals` shows the tilted normals. A material without a map
  renders exactly as before, and so does a flat map, to the last bit.
- **MTL `map_bump` and `bump`**, with `-bm` as the height in the model's
  units. Height maps are read as numbers, byte / 255, not decoded from sRGB
  as colours are.
- **A height map to try it with**: `models/dimples.png`, a golf ball's skin,
  512 x 512 and 8 KB, written by `scripts/make-dimples.py`. A bare name
  resolves in `models/`: `raymini-cli spot --bump dimples`.
- **A Bumps picker in the viewer**, over the `.png` files of the models
  directory (or the file's own map, or none), with the height beside it.
- Goldens `spot_dimpled_lit` and `spot_dimpled_normals`; ctest `cli_bump`,
  `cli_bump_normals`, `cli_bad_bump`, `cli_bump_without_coordinates`.

Not yet: the surface does not move, so silhouettes stay smooth and bumps
neither shadow nor hide each other, the limit the paper states itself. Only
OBJ meshes carry the coordinates a map follows: the teapot, the rams, the
ground plane and the analytic spheres stay smooth, and `--bump` says so.

### Fixed

- **A greyscale picture loads as what it is.** `Image::load` kept the single
  channel of a grey PNG and called it RGB, so every pixel was read three
  bytes at a time and two thirds of them past the end of the buffer: such a
  file as `--texture` gave coloured stripes. stb now spreads the grey over
  the three channels. No bundled picture was grey, so nothing showed it.

## [0.6.0] - 2026-10-03

Step 13 of the timeline: J. F. Blinn & M. E. Newell, "Texture and Reflection
in Computer Generated Images", Communications of the ACM 19(10), 1976 — the
direction of a reflected ray, instead of a position on the surface, picks
the texel.

### Added

- **Environment maps.** The world around the scene is one panoramic
  picture, `--environment <file>`: every ray that leaves the scene reads it
  where it points, azimuth across and polar angle down, bilinearly
  (`src/core/Environment.h`). It shows behind the model, and in every
  mirror and every glass, which until now reflected a black sky. Direction
  alone picks the texel, so the world is infinitely far, as in the paper.
  Lit mode only: the analysis modes keep their flat background, and a scene
  with no map renders exactly as before.
- **Radiance maps keep their range.** A `.hdr` panorama is taken as the
  linear radiance it holds, the sun far above 1, and goes through the
  display like any other radiance; other formats are decoded from sRGB.
  `Texture` reads `.hdr` the same way.
- **A world to try it in**: `models/venice_sunset.hdr`, Greg Zaal's Venice
  Sunset from Poly Haven (CC0), 1024 x 512, 1.4 MB. A bare name resolves in
  `models/` like a model's: `--environment venice_sunset`.
- **A World picker in the viewer**, over the `.hdr` files of the models
  directory.
- Golden `teapot_chrome_venice_lit`, the chrome teapot of the paper's
  figure 8; ctest `cli_environment`, `cli_bad_environment`.

Not yet: the map is seen, it does not light. Matte surfaces are still lit by
the rig alone; lighting by the map is step 46.

### Fixed

- **`--sphere ... mirror` is a mirror.** The options that describe the model
  were applied after the spheres had joined the scene, so `--reflectivity`,
  0 unless given, wiped the mirror off every sphere, and `--transparency`,
  `--color`, `--texture`, `--specular` and `--shininess` reached them too.
  The gallery's step 11 showed a dark ball, and its caption blamed the black
  sky. Those options now touch the model alone, and the render line lists
  the mirrors and the glass the scene really holds (ctest
  `cli_mirror_sphere`). Since v0.2.0.
- **The release archive ships Spot's texture.** `scripts/package.sh` copied
  the meshes and their MTL files and no picture, so the Spot of the v0.5.0
  archive came out untextured, with a warning and exit 0. It copies
  `models/*.png` too, and CI renders Spot from the unpacked archive and
  fails on any warning.

## [0.5.0] - 2026-09-20

Step 12 of the timeline: E. Catmull, "A Subdivision Algorithm for Computer
Display of Curved Surfaces", PhD thesis, University of Utah, 1974, chapter
6 — the surface carries its own frame, and a picture is read through it.

### Added

- **Textures.** The `vt` of an OBJ file, until now parsed and thrown away,
  are kept, interpolated over the triangle by the same barycentric weights
  as the normal, and read from an image: `map_Kd` in the MTL, or
  `--texture <file>` on the command line. The read is bilinear, texels are
  decoded from sRGB to linear when the image loads, coordinates outside
  [0, 1] wrap, and the material's Kd tints the result
  (`src/core/Texture.h`).
- **A uv mode** that paints the coordinates, u to red and v to green: what
  an unwrapping looks like before anything is mapped onto it.
- **Spot arrives dressed**: `models/spot.mtl` ties her mesh to her texture,
  which Keenan Crane's archive leaves as two unrelated files. Goldens
  `spot_textured_lit` and `spot_uv`.

### Fixed

- The analysis modes no longer go through the filmic display. A normal, a
  distance, a coordinate is not light: metering it and bending it through a
  tone curve was lying about it. Only Lit is displayed that way now; the
  others keep their raw values, as they did before v0.2.0.

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

[Unreleased]: https://github.com/alexduros/RayTracer/compare/v0.6.0...HEAD
[0.6.0]: https://github.com/alexduros/RayTracer/releases/tag/v0.6.0
[0.5.0]: https://github.com/alexduros/RayTracer/releases/tag/v0.5.0
[0.4.0]: https://github.com/alexduros/RayTracer/releases/tag/v0.4.0
[0.3.0]: https://github.com/alexduros/RayTracer/releases/tag/v0.3.0
[0.2.0]: https://github.com/alexduros/RayTracer/releases/tag/v0.2.0
[0.1.0]: https://github.com/alexduros/RayTracer/releases/tag/v0.1.0
