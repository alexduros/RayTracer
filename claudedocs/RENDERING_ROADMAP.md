# Rendering roadmap

What the raytracer can offer next, mode by mode, towards richer scenes and
bigger models. Every entry has a **Principle** zone: one sentence saying how
the picture is made. The same sentences live in `RayTracer::plannedMode()`,
so the viewer lists them greyed out under "Planned" in the mode menu (hover
for the principle) and `raymini-cli --help` prints them. `EXPERIMENTS.md` is
the implementation order with tests; this page is the map.

## Available today

| Mode | Principle |
|------|-----------|
| Lit (Lambert) | Each light adds material colour x light colour x the cosine between the normal and the light direction, plus a constant ambient term. |
| Blinn-Phong specular | A white highlight where the half-vector between the light and view directions lines up with the normal, raised to the material's shininess. |
| Hard shadows | A shadow ray from the hit point toward each light drops that light when any surface blocks it. |
| Ground plane | A large backdrop quad at the bottom of the model's box that catches its shadows without changing the framing. |
| Ambient (albedo) | The material's base colour at the hit, with no lighting at all. |
| Hit mask (coverage) | White where the primary ray hits geometry, black where it escapes. |
| Normals | The surface normal remapped from [-1, 1] to [0, 1] per axis as red, green, blue. |
| Depth | The eye-to-hit distance mapped linearly between near and far, white to dark grey. |
| Object id | One palette colour per object, so a pixel tells which surface it belongs to. |
| Anti-aliasing | Several rays per pixel on a sub-pixel grid, optionally jittered, averaged in linear colour. |
| Soft shadows | Many shadow rays over each light's disk measure the fraction of it that is visible. |
| Mirror reflections | A reflective surface blends in what a ray mirrored about its normal sees, recursively up to a depth. |
| Ambient occlusion | Hemisphere rays measure how open each point's surroundings are; a mode of its own and a factor on the ambient and diffuse light. |
| Refraction (glass) | A transparent surface splits the light between reflection and a ray bent by Snell's law, in the Fresnel proportions. |
| Tone mapping and exposure | Radiance stays in floats; a metered exposure, a tone curve and sRGB map it to the screen last. |
| Analytic primitives | A sphere, a cylinder or a disc is an equation: the ray meets it at the root of a polynomial, exactly, at any zoom. |

## 1. Local shading

### Blinn-Phong specular (done)
**Principle:** Adds a highlight where the half-vector between the light and view directions lines up with the normal, sharpened by a shininess exponent.
Done: `Material::shininess` (MTL `Ns`), `RayTracer::setSpecularEnabled`, tests in `tests/TestShading.cpp` (peaks with the light at the eye, shrinks with shininess, white whatever the material colour). Reference: Phong 1975; Blinn, "Models of Light Reflection for Computer Synthesized Pictures", SIGGRAPH 1977. (Experiment 2)

### Hard shadows (done)
**Principle:** A shadow ray from the hit point toward each light drops that light's contribution when any surface blocks it.
Done: `RayTracer::setShadows`, shadow origin offset along the normal by 1e-4 of the model size, `Scene::addGroundPlane` (CLI `--ground`, GUI "Ground"). Tests: a point behind a cube gets the ambient term only, its neighbour ambient + diffuse, a lone plane never shadows itself, the ground catches the model's shadow; golden `teapot_ground_*`. Reference: Appel 1968; Whitted 1980. (Experiment 1)

### Soft shadows (area lights) (done)
**Principle:** Many shadow rays toward points spread over the light's disk estimate the fraction of it that is visible, giving penumbrae instead of hard edges.
Done as n x n jittered shadow rays over each light's disk (`--shadow-samples`, `--light-radius`): binary with one ray or radius 0, and across a test penumbra the visible share matches the uncovered area of the disk and never decreases. Reference: Cook, Porter & Carpenter, "Distributed Ray Tracing", SIGGRAPH 1984; Shirley & Chiu, "A Low Distortion Map Between Disk and Square", JGT 1997. (Experiment 6)

### Ambient occlusion (done)
**Principle:** Rays cast over the hemisphere around the normal measure how open the surroundings are, darkening creases and contact points.
Done: `RayTracer::setAmbientOcclusion(n, radius)`, n x n cosine-weighted rays (a jittered grid on the unit disk lifted onto the hemisphere) with a maximum length; the open share scales the ambient and diffuse terms in Lit and is shown as grey by the new `ao` mode (CLI `--ao`, `--ao-radius`, `--mode ao`; GUI Occlusion, AO radius). Tests: a lone plane is open everywhere, next to a wall the open share is the uncovered part of the unit disk (1/2 in the corner, 1 from one radius away), the spread over seeds shrinks with the samples, the highlight is untouched; goldens `teapot_ground_ao4_lit` and `_ao`. Reference: Zhukov, Iones & Kronin, "An Ambient Light Illumination Model", EGWR 1998. (Experiment 8)

### Physically based materials (GGX)
**Principle:** A microfacet model shapes the highlight from a statistical distribution of tiny mirrors with Fresnel and masking terms, driven by roughness and metalness.
Needs: roughness and metalness on `Material`; supersedes Blinn-Phong. Test: energy never exceeds 1 (white furnace), roughness 0 tends to a mirror, roughness 1 to Lambert. Reference: Cook & Torrance, SIGGRAPH 1981; Walter et al., "Microfacet Models for Refraction through Rough Surfaces", EGSR 2007.

## 2. Light transport

### Mirror reflections (done)
**Principle:** Rays bounce off reflective surfaces recursively and add what they see, scaled by the material's reflectivity, up to a depth limit.
Done: `Material::reflectivity`, `RayTracer::setMaxDepth` (CLI `--reflectivity`, `--ground-reflectivity`, `--max-depth`; GUI Ground mirror, Mirror, Bounces); colour = (1 - k) x own shading + k x the mirrored ray's colour. Tests: a perfect mirror shows the red panel its reflected ray meets, a quarter mirror blends exactly, depth 0 gives the plane's own colour bit for bit, two facing mirrors stay between their colours and converge by k^depth to the closed form, a mirror floor shows the red cube standing on it; golden `teapot_ground_mirror_lit`. Reference: Whitted 1980. (Experiment 7)

### Refraction (glass) (done)
**Principle:** Rays bend through transparent surfaces following Snell's law and split between reflection and transmission by the Fresnel term.
Done: `Material::transparency` and `ior` (MTL `d`, `Tr`, `Ni`), two-sided rays inside the object so they can leave it, the exact Fresnel equations (`src/core/Optics.h`) rather than Schlick's approximation, which is not 0 between equal indices. CLI `--transparency`, `--ior`; GUI Glass, Index. Tests: Snell and the critical angle, Fresnel at normal incidence, grazing, Brewster's angle and reciprocity; a slab of index 1 is invisible; a slab shifts the edge under it by thickness x (tan i - tan t) for water, glass and diamond; what comes through is (1 - F_in)(1 - F_out) of the floor; golden `teapot_ground_glass_lit`. Reference: Whitted 1980; Born & Wolf, *Principles of Optics*. (Experiment 7)

### Emissive materials and mesh lights
**Principle:** Surfaces emit light themselves, so any mesh can be a lamp, and sampling them directly keeps soft lighting quiet.
Needs: an emission colour on `Material`, light sampling over triangles. Test: an emissive quad lights a plane with the analytic falloff of a disk source. Reference: Shirley, Wang & Zimmerman, "Monte Carlo Techniques for Direct Lighting Calculations", ACM TOG 15(1), 1996.

### Path tracing (global illumination)
**Principle:** Each pixel averages many random light paths bouncing through the scene, converging on the rendering equation with indirect light and colour bleeding.
Needs: emissive materials, many samples per pixel; the BVH and the progressive display are done. Test: a white furnace converges to the emission, a Cornell box shows red and green bleeding on the white walls. Reference: Kajiya, "The Rendering Equation", SIGGRAPH 1986.

### Environment lighting (HDR sky)
**Principle:** A panoramic image lights the scene: rays that miss geometry return the sky's colour and diffuse surfaces integrate it over the hemisphere.
Needs: HDR loading (stb reads `.hdr`), equirectangular lookup, hemisphere sampling. Test: a uniform white environment lights a sphere evenly, a single bright texel behaves like a distant point light. Reference: Debevec, "Rendering Synthetic Objects into Real Scenes", SIGGRAPH 1998.

## 3. Materials and textures (for complex models)

### Textures, procedural and image
**Principle:** The material colour becomes a function of the hit: a checker or noise of the position, or a bitmap looked up through interpolated texture coordinates.
Needs: keep `vt` from OBJ files and `map_Kd` from MTL, barycentric interpolation of UVs. Test: a checker plane alternates at the right period, a 2x2 image texture puts each colour in its quadrant. Reference: Catmull 1974; Perlin, "An Image Synthesizer", SIGGRAPH 1985.

### Normal and bump mapping
**Principle:** A texture perturbs the shading normal per pixel to fake fine surface detail without adding geometry.
Needs: textures, tangent frames per triangle. Test: a flat quad with a bumped normal map shades as if grooved, a flat map leaves it flat. Reference: Blinn, "Simulation of Wrinkled Surfaces", SIGGRAPH 1978; Cohen, Olano & Manocha, SIGGRAPH 1998.

## 4. Camera and image

### Depth of field
**Principle:** Rays start from random points on a lens disk and converge on the focal plane, blurring whatever lies nearer or farther.
Needs: aperture and focus distance on `Camera`, per-pixel sampling. Test: a quad on the focal plane keeps hard edges, one far from it blurs, aperture 0 reproduces the pinhole goldens. Reference: Potmesil & Chakravarty, SIGGRAPH 1981; Cook, Porter & Carpenter 1984. (Experiment 10)

### Tone mapping and exposure (done)
**Principle:** Radiance stays linear in floats and is mapped to the display range by an exposure and a tone curve before sRGB encoding, so bright scenes no longer clip.
Done: `HdrImage` between the tracer and the bytes, `Display` (exposure in stops, metered or set; no curve, Reinhard or ACES; linear, sRGB or gamma), presets `filmic` (the CLI and viewer default) and `linear` (the tracer's default and the goldens'), `.hdr` output. Tests: sRGB 0.5 -> 188, exposure in stops, curves keep gradations above 1, Reinhard's white point and hue, metering to the key 0.18, RGBE round trip, tiles and threads give the same floats and bytes. Reference: Reinhard, Stark, Shirley & Ferwerda, SIGGRAPH 2002. (Experiment 9)

### Motion blur
**Principle:** Each ray samples a random instant of the shutter interval while objects or the camera move through it, smearing motion.
Needs: a time parameter on `Camera` and per-object transforms over time; comes after instancing. Reference: Cook, Porter & Carpenter 1984.

## 5. Analysis modes for complex models

### Wireframe overlay
**Principle:** Pixels whose barycentric coordinates lie close to a triangle edge are drawn dark over the shaded image, showing the tessellation of the model.
Needs: `Ray::hit` returning its barycentric coordinates. Test: a single triangle renders a dark outline of constant pixel width. Reference: Baerentzen et al., "Single-pass Wireframe Rendering", SIGGRAPH 2006 Sketches.

### Triangle id and barycentrics
**Principle:** Each triangle gets its own colour, or its barycentric weights are shown as red, green and blue, to inspect the mesh structure and interpolation.
Needs: the triangle index from `Ray::nearestHit`. Reference: the item buffer, Weghorst, Hooper & Greenberg, ACM TOG 3(1), 1984.

### UV view
**Principle:** Texture coordinates are shown as red (u) and green (v) to check an unwrapping before texturing.
Needs: UVs kept from OBJ files.

### Cost heatmap
**Principle:** Each pixel is coloured by how many triangle or bounding-box tests its rays needed, showing where the time goes on complex models.
Needs: box and triangle test counters in `closestHit` and the BVH traversal; it shows what the BVH saves and where it cannot. Reference: Wald, PhD thesis, Saarland University, 2004.

## 6. Scene and scale (what complex scenes need)

### Bounding-volume hierarchy (done)
**Principle:** A tree of bounding boxes lets each ray skip almost every triangle, turning minutes into milliseconds on large models.
Done as one tree per object (median split, leaves of 4): hits identical to brute force on random rays, edge-aimed rays and whole renders; the minion on its ground renders in 17 ms instead of 60 s. Reference: Kay & Kajiya, "Ray Tracing Complex Scenes", SIGGRAPH 1986; Wald, "On Fast Construction of SAH-based Bounding Volume Hierarchies", RT 2007. (Experiment 3)

### Tile-parallel rendering (done)
**Principle:** Several workers pull tiles from a shared counter so every core traces at once.
Done: `RenderJob` runs one worker per core by default (CLI `--threads`, GUI Threads); pixels and statistics are byte-identical to the synchronous render for any tile size and thread count, and a cancel leaves each tile final or pending. Ram with 8x8 soft shadows at 384x384: 3.9 s -> 0.75 s on ten cores. (Experiment 4)

### Scene description file
**Principle:** A text file places several models with transforms, materials, lights and the camera, so a complex scene is data rather than code.
Needs: object transforms (see instancing), a small JSON or TOML reader. Test: a two-model scene loads with the expected object count and bounding box.

### Instancing and transforms
**Principle:** An object carries a transform, so one mesh can appear many times at different positions, sizes and orientations without copying it.
Needs: transforming rays into object space in `closestHit`. Test: two instances of the teapot give twice the hits of one.

### Ground plane (done)
**Principle:** A large quad under the model receives shadows and gives every render a floor, recreating the look of the original `Rendu.png`.
Done as a backdrop object that the bounding box ignores. (Part of experiment 1)

## Suggested order

1. Ground plane + hard shadows, then Blinn-Phong: the scene starts to look like a scene. **Done.**
2. BVH, then tile threads: the cat and the minion become interactive. **Done**: the minion renders in milliseconds, and threads keep the sampled effects fast.
3. Soft shadows, ambient occlusion, depth of field: all reuse the sampling loop. **Soft shadows and ambient occlusion done**, each with its own per-pixel `Sampler` stream; depth of field will add a third.
4. Reflections and refraction, then textures from OBJ UVs. **Reflections and refraction done.**
5. Tone mapping, then environment lighting and path tracing. **Tone mapping done**; from here on, `RAY_TRACING_TIMELINE.md` sets the order.
6. Analysis modes (wireframe, cost heatmap) now that the BVH exists; scene files and instancing when there is more than one thing to place.
