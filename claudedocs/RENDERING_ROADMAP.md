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
| Ambient (albedo) | The material's base colour at the hit, with no lighting at all. |
| Hit mask (coverage) | White where the primary ray hits geometry, black where it escapes. |
| Normals | The surface normal remapped from [-1, 1] to [0, 1] per axis as red, green, blue. |
| Depth | The eye-to-hit distance mapped linearly between near and far, white to dark grey. |
| Object id | One palette colour per object, so a pixel tells which surface it belongs to. |
| Anti-aliasing | Several rays per pixel on a sub-pixel grid, optionally jittered, averaged in linear colour. |

## 1. Local shading

### Blinn-Phong specular
**Principle:** Adds a highlight where the half-vector between the light and view directions lines up with the normal, sharpened by a shininess exponent.
Needs: shininess on `Material` (MTL `Ns`). Test: the highlight pixel is brighter than its neighbours, shrinks with shininess, and specular 0 reproduces the Lambert golden. Reference: Phong 1975; Blinn, "Models of Light Reflection for Computer Synthesized Pictures", SIGGRAPH 1977. (Experiment 2)

### Hard shadows
**Principle:** A shadow ray from the hit point toward each light drops that light's contribution when any surface blocks it.
Needs: a ground plane under the model (`Scene::addGroundPlane`), an epsilon offset along the normal. Test: the pixel under an occluder equals the ambient term, its neighbour equals ambient + diffuse, a bare plane has no acne. Reference: Appel 1968; Whitted 1980. (Experiment 1)

### Soft shadows (area lights)
**Principle:** Many shadow rays toward points spread over the light's disk estimate the fraction of it that is visible, giving penumbrae instead of hard edges.
Needs: hard shadows, the per-pixel sampling from anti-aliasing, `Light::radius` (already stored). Test: radius 0 gives a binary shadow, the penumbra width grows with the radius and is monotonic across it. Reference: Cook, Porter & Carpenter, "Distributed Ray Tracing", SIGGRAPH 1984. (Experiment 6)

### Ambient occlusion
**Principle:** Rays cast over the hemisphere around the normal measure how open the surroundings are, darkening creases and contact points.
Needs: cosine-weighted hemisphere sampling, a maximum ray length, a BVH to stay fast. Test: the inner corner of two quads is darker than open plane, a lone plane is 1 everywhere, variance drops with samples. Reference: Zhukov, Iones & Kronin, "An Ambient Light Illumination Model", EGWR 1998. (Experiment 8)

### Physically based materials (GGX)
**Principle:** A microfacet model shapes the highlight from a statistical distribution of tiny mirrors with Fresnel and masking terms, driven by roughness and metalness.
Needs: roughness and metalness on `Material`; supersedes Blinn-Phong. Test: energy never exceeds 1 (white furnace), roughness 0 tends to a mirror, roughness 1 to Lambert. Reference: Cook & Torrance, SIGGRAPH 1981; Walter et al., "Microfacet Models for Refraction through Rough Surfaces", EGSR 2007.

## 2. Light transport

### Mirror reflections
**Principle:** Rays bounce off reflective surfaces recursively and add what they see, scaled by the material's reflectivity, up to a depth limit.
Needs: reflectivity on `Material`, recursion in `shade()` with a depth counter. Test: a mirror plane facing a red quad shows red where the reflection lands, depth 0 shows the plane's own colour. Reference: Whitted 1980. (Experiment 7)

### Refraction (glass)
**Principle:** Rays bend through transparent surfaces following Snell's law and split between reflection and transmission by the Fresnel term.
Needs: reflections, index of refraction (MTL `Ni`), transparency (MTL `d`). Test: a slab of index 1 is invisible, a slab of index 1.5 shifts a background edge by the predicted offset. Reference: Whitted 1980; Schlick, CGF 13(3), 1994. (Experiment 7)

### Emissive materials and mesh lights
**Principle:** Surfaces emit light themselves, so any mesh can be a lamp, and sampling them directly keeps soft lighting quiet.
Needs: an emission colour on `Material`, light sampling over triangles. Test: an emissive quad lights a plane with the analytic falloff of a disk source. Reference: Shirley, Wang & Zimmerman, "Monte Carlo Techniques for Direct Lighting Calculations", ACM TOG 15(1), 1996.

### Path tracing (global illumination)
**Principle:** Each pixel averages many random light paths bouncing through the scene, converging on the rendering equation with indirect light and colour bleeding.
Needs: emissive materials, a BVH, many samples per pixel, the progressive display (done). Test: a white furnace converges to the emission, a Cornell box shows red and green bleeding on the white walls. Reference: Kajiya, "The Rendering Equation", SIGGRAPH 1986.

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

### Tone mapping and exposure
**Principle:** Radiance stays linear in floats and is mapped to the display range by an exposure and a tone curve before sRGB encoding, so bright scenes no longer clip.
Needs: a floating-point image buffer between the tracer and the 8-bit output. Test: linear 0.5 encodes to sRGB 188, values above 1 no longer clip flat, exposure 2 doubles the linear value. Reference: Reinhard, Stark, Shirley & Ferwerda, SIGGRAPH 2002. (Experiment 9)

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
Needs: counters in `closestHit` and in the BVH; it is the diagnostic that justifies experiment 3. Reference: Wald, PhD thesis, Saarland University, 2004.

## 6. Scene and scale (what complex scenes need)

### Bounding-volume hierarchy
**Principle:** A tree of bounding boxes lets each ray skip almost every triangle, turning minutes into seconds on large models.
Test: identical pixels to brute force on 10 000 random rays, minion at 256x256 under a second. Reference: Kay & Kajiya, "Ray Tracing Complex Scenes", SIGGRAPH 1986; Wald, "On Fast Construction of SAH-based Bounding Volume Hierarchies", RT 2007. (Experiment 3)

### Tile-parallel rendering
**Principle:** Several workers pull tiles from a shared counter so every core traces at once; `RenderJob` already owns the tiles.
Test: byte-identical to one worker. (Experiment 4)

### Scene description file
**Principle:** A text file places several models with transforms, materials, lights and the camera, so a complex scene is data rather than code.
Needs: object transforms (see instancing), a small JSON or TOML reader. Test: a two-model scene loads with the expected object count and bounding box.

### Instancing and transforms
**Principle:** An object carries a transform, so one mesh can appear many times at different positions, sizes and orientations without copying it.
Needs: transforming rays into object space in `closestHit`. Test: two instances of the teapot give twice the hits of one.

### Ground plane
**Principle:** A large quad under the model receives shadows and gives every render a floor, recreating the look of the original `Rendu.png`.
(Part of experiment 1)

## Suggested order

1. Ground plane + hard shadows, then Blinn-Phong: the scene starts to look like a scene.
2. BVH, then tile threads: the cat and the minion become interactive.
3. Soft shadows, ambient occlusion, depth of field: all reuse the sampling loop.
4. Reflections and refraction, then textures from OBJ UVs.
5. Tone mapping, then environment lighting and path tracing.
6. Analysis modes (wireframe, cost heatmap) as soon as the BVH exists; scene files and instancing when there is more than one thing to place.
