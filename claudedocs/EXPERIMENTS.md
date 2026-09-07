# Next ten experiments

The map of every mode the raytracer could offer, with a one-sentence
principle each, is `RENDERING_ROADMAP.md`; this file is the implementation
order. Ordered so that each one builds on the previous ones. Every experiment ships
with (a) a unit test on synthetic geometry, (b) regenerated goldens reviewed
by eye, (c) a `raymini-cli` flag, (d) a PNG to look at.

Timings quoted are for an M-series Mac, single thread, brute-force
intersection unless stated otherwise.

## 1. Ground plane and hard shadows (done)

Recreate the look of `Rendu.png`: a model standing on a plane, casting a
shadow. Add `Scene::addGroundPlane(material)` (a large quad at the bottom of
the bounding box, optionally with a world-space checkerboard so the plane
reads even without shadows). In `shade`, before adding a light's diffuse term,
cast a shadow ray from `hit + epsilon * normal` toward the light and skip the
light if `closestHit` finds anything closer than the light.

- Test: a small quad above a large quad, one light straight above. The pixel
  under the small quad equals the ambient term only; a pixel beside it equals
  ambient + diffuse. No self-shadowing acne on a flat plane (every pixel of an
  unoccluded plane is lit).
- CLI: `--ground`, `--shadows`.
- Cost: shadow rays multiply intersection work by (1 + lights); fine on teapot,
  slow on minion until experiment 3.
- Done: `Scene::addGroundPlane` (a backdrop the bounding box ignores),
  `RayTracer::setShadows`, CLI `--ground` / `--no-shadows`, GUI "Ground" and
  "Shadows"; tests in `tests/TestShading.cpp`, golden `teapot_ground_*`. The
  checkerboard waits for procedural textures.

## 2. Blinn-Phong specular (done)

Use `Material::specular` plus a new shininess exponent. Add
`specular * intensity * pow(max(0, n.h), shininess) * lightColor` with
`h = normalize(l + v)`.

- Test: a quad facing the camera, light next to the camera: the pixel where
  the reflection direction hits the eye is brighter than its neighbours, the
  highlight shrinks when shininess grows, and a specular of 0 reproduces the
  Lambert golden exactly.
- CLI: `--specular <k> --shininess <n>`.
- Done: `Material::shininess` (MTL `Ns`, default 32), `RayTracer::setSpecularEnabled`,
  CLI `--no-specular`, GUI "Specular"; tests in `tests/TestShading.cpp`.
  Per-material overrides on the CLI wait for the material editor.

## 3. BVH acceleration structure

Replace the removed KdTree with a bounding-volume hierarchy over triangles:
median split on the longest axis, 4 to 8 triangles per leaf, flat node array,
built once per object in `Object`. `RayTracer::closestHit` traverses it with
the existing `Ray::intersect(BoundingBox)` slab test.

- Test: for 10 000 random rays against teapot and ram, BVH and brute force
  return the same object index and the same distance within 1e-5. Golden
  images must be identical (tolerance 0 for this commit). Minion at 256x256
  in under 1 s (today: about 25 s).
- CLI: `--no-bvh` to compare.
- This is the enabler for everything sampled below.

## 4. Tile-parallel rendering

Split the image into 32x32 tiles, hand them out through an atomic counter to
`std::thread::hardware_concurrency()` workers. Stats are accumulated per
thread and merged. Most of the plumbing exists: `RenderJob` already traces
32x32 tiles on one worker, publishes them under a mutex and exposes
progress/cancel, and `Stats::accumulate` merges per-tile counters. What is
left is N workers pulling tile indices from an atomic counter, each with a
private `Stats` merged at the end.

- Test: the image is byte-identical to `RayTracer::render()`; stats (rays,
  hits, min/max distance) match. The one-worker version of this test is in
  `tests/TestRenderJob.cpp`; extend it to N workers and report the speedup.
- CLI: `--threads <n>` (0 = auto). GUI: render time drops enough to make
  "Render Scene" feel interactive on the minion.

## 5. Supersampling anti-aliasing (done)

Implemented: `RayTracer::setAntiAliasing(n, jitter)` traces an n x n
sub-pixel grid, optionally jittered with a per-pixel seed
(`std::minstd_rand`, raw engine output so every platform agrees), averaged
in linear colour. CLI `--aa <n> --jitter`; GUI Anti-alias / Jitter in the
Render section (2x2 by default).

- Tests (`tests/TestAntiAliasing.cpp`): a pixel 25 % covered by a quad is 0
  at 1 ray and 64 at 4x4 regular; a 50 % covered pixel is 128 (linear
  average); interior pixels unchanged; jitter reproducible and different
  from the grid; `tests/TestRenderJob.cpp` checks jitter is tile-order
  independent. Goldens `teapot_aa2_*` and `teapot_aa2j_*`.
- Left for later: adaptive sampling (only refine pixels whose sub-samples
  disagree) and a proper reconstruction filter instead of the box average.

## 6. Soft shadows from area lights

`Light::radius` already exists. Sample points on a disk facing the shaded
point (same RNG as experiment 5), count the fraction of unoccluded shadow
rays, scale the light's contribution by it.

- Test: a plane under an occluder with a light of radius 0 gives a binary
  shadow; with radius r the penumbra width grows with r and the shadow value
  is monotonic across the penumbra.
- CLI: `--shadow-samples <n>`, `--light-radius <r>`.

## 7. Mirror reflections (and refraction as a stretch)

Add `Material::reflectivity`; in `shade`, if it is positive, trace a
reflected ray recursively up to `maxDepth` and blend. Refraction follows the
same recursion with Snell's law and an index of refraction.

- Test: a mirror plane facing a red quad: the pixel where the reflection
  lands is red; with `maxDepth = 0` it is the plane's own colour; total
  contribution never exceeds 1 (energy conservation).
- CLI: `--max-depth <n>`, per-object material overrides.

## 8. Ambient occlusion

Per-pixel hemisphere sampling around the normal (cosine-weighted, seeded
RNG), with a maximum ray length so open scenes stay bright. Multiplies the
ambient and diffuse terms. This replaces the removed per-vertex code, which
sampled one octant only.

- Test: a point in the inner corner of two perpendicular quads is darker
  than a point in the open; a flat plane alone has an occlusion factor of 1
  everywhere; the result converges as samples increase (variance drops).
- CLI: `--ao <samples> --ao-radius <r>`.

## 9. Linear pipeline, exposure and tone mapping

Keep the float image, then apply exposure, a tone mapper (Reinhard or ACES)
and sRGB encoding before quantizing. Add light and material overrides on the
CLI so the default cyan/yellow/white rig can be tuned without code changes.

- Test: linear 0.5 encodes to sRGB 188; values above 1 no longer clip to a
  flat 255 with tone mapping; exposure 2 doubles the linear value before
  encoding; `--gamma 1` reproduces the old goldens.
- CLI: `--exposure <ev> --tonemap none|reinhard|aces --light <i> <x> <y> <z> <r> <g> <b> <intensity>`.

## 10. Depth of field

Thin-lens camera: `Camera::aperture` and `focusDistance`; each primary ray
starts from a random point on the lens disk and passes through the focus
point of the pinhole ray. Uses the sampling infrastructure from experiment 5.

- Test: a quad on the focus plane keeps hard edges (edge pixels stay 0/255);
  a quad far from it blurs (edge pixels become intermediate); aperture 0
  reproduces the pinhole goldens exactly.
- CLI: `--aperture <r> --focus <d>`.

## After these

Textures: OBJ files carry UVs (parsed, not yet stored) and MTL `map_Kd`
names, so image textures are within reach; procedural checker and noise
come first for the OFF models, which have no UVs. Also an interactive
material/light editor in the GUI, and a progressive preview in the
raytracer panel (render at 64x64 first, then refine).
