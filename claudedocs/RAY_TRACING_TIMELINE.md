# Ray tracing, paper by paper

The plan after experiment 10: walk through the research on ray tracing in
chronological order, from Appel (1968) to this year, and turn each landmark
paper into a step of raymini. Effects, performance, hardware, modelling,
sampling, images: everything that shaped the field is on the list, each
step built the way the first ten were (`EXPERIMENTS.md`): a test on
synthetic geometry that proves the physics or the invariant first, a
`RayTracer` setter and a `raymini-cli` flag, a golden, a picture of the ram
in the step-by-step gallery.

## How to read it

- One step per paper, or per tight group of papers on the same idea, placed
  at the year of the first one. Numbers follow on from `EXPERIMENTS.md`
  (11, 12, ...) in the order of the timeline.
- Each step gives the paper(s), what they brought, what raymini builds from
  them (**Here**), the test that proves it (**Proof**), a size and a
  category.
- Sizes: **S** one session, **M** one or two, **L** several, **XL** a
  project in itself. Categories: effect, modelling, performance, sampling,
  image, hardware, real time, inverse.
- ★ marks the backbone: the steps that change what raymini can render or
  how fast. The rest are side branches worth taking but skippable.
- Steps already done are listed in their place (✓) so the timeline reads
  whole.
- Where a later paper is needed as a tool earlier, the step says so rather
  than breaking the order.

## Before the timeline: experiments 9 and 10

- ✓ **9 · Linear pipeline, exposure, tone mapping** (done). It comes
  first, out of order (Reinhard et al., SIGGRAPH 2002): every later light
  transport step produces radiance above 1, and a float frame buffer is the
  tool they all need. It also covers Ward's "Real Pixels" (Graphics Gems II,
  1991: HDR pixels, `.hdr` in and out) and can offer Tumblin & Rushmeier's
  operator (IEEE CG&A 13(6), 1993), the first tone reproduction paper.
- **10 · Depth of field** (as specified) moves to its place in the timeline,
  1981, between steps 16 and 17.

## Era I · Foundations (1968–1985)

**✓ 1968** · A. Appel, "Some Techniques for Shading Machine Renderings of
Solids", AFIPS 1968. Ray casting and shadow rays: experiment 1.

### 11 · 1971 · Analytic primitives ★
R. A. Goldstein & R. Nagel, "3-D Visual Simulation", *Simulation* 16(1),
1971 (MAGI's SynthaVision). Rays against exact quadrics instead of polygons.
- **Here:** a primitive interface next to `Mesh`: spheres, planes, cylinders,
  discs, intersected in closed form, in the BVH as boxes.
- **Proof:** a sphere's hit distance is the root of the quadratic to 1e-6;
  its silhouette is a circle; a tessellated sphere converges to it.
- **M** · modelling. Enables 17, 18 and every analytic test scene.

### 12 · 1974 · Texture mapping ★
E. Catmull, "A Subdivision Algorithm for Computer Display of Curved
Surfaces", PhD thesis, University of Utah, 1974.
- **Here:** keep OBJ `vt`, read MTL `map_Kd` (stb), interpolate UVs with
  the barycentrics `Ray::hit` computes, bilinear lookup; a UV view mode.
- **Proof:** a 2x2 texture puts each colour in its quadrant; a checker
  alternates at its period; no texture = today's picture.
- **M** · effect.

**✓ 1975, 1977** · B. T. Phong, CACM 18(6); J. Blinn, SIGGRAPH 1977.
Experiment 2.

### 13 · 1976 · Environment maps ★
J. Blinn & M. Newell, "Texture and Reflection in Computer Generated
Images", CACM 19(10), 1976.
- **Here:** a latitude-longitude image returned by every ray that escapes,
  so mirrors and glass reflect a world instead of black.
- **Proof:** a ray leaving along d reads the texel at (atan2, acos) of d; a
  mirror sphere shows the map mirrored as predicted.
- **S** · effect. Lighting by the same map comes in 46.

### 14 · 1978 · Bump mapping
J. Blinn, "Simulation of Wrinkled Surfaces", SIGGRAPH 1978.
- **Here:** perturb the shading normal from a height map (12) or noise (25).
- **Proof:** a flat height map changes no pixel; a ramp tilts the normal by
  atan(slope).
- **S** · effect.

### 15 · 1979 · Tinted glass and shadows through it
D. S. Kay & D. Greenberg, "Transparency for Computer Synthesized Images",
SIGGRAPH 1979: transmission falls off with the thickness crossed.
- **Here:** absorption along the path inside glass (Beer-Lambert, a colour
  per unit length), and shadow rays through glass filtered by its
  transmission instead of blocked, so glass stops casting black shadows (our
  extension of the idea).
- **Proof:** a slab of absorption σ and thickness d passes exp(-σd); a pane
  of index 1 casts no shadow; clear glass at normal incidence passes
  (1 - 0.04)².
- **S** · effect.

**✓ 1980** · T. Whitted, "An Improved Illumination Model for Shaded
Display", CACM 23(6), 1980: supersampling, mirrors, refraction (experiments
5, 7, 7b). S. Rubin & T. Whitted, SIGGRAPH 1980: bounding hierarchies (see
1986).

### 16 · 1981 · Microfacet reflection ★
R. Cook & K. Torrance, "A Reflectance Model for Computer Graphics",
SIGGRAPH 1981.
- **Here:** Beckmann distribution, geometric attenuation and the Fresnel
  term of 7b as a second specular model next to Blinn-Phong.
- **Proof:** f(l, v) = f(v, l); the normal distribution integrates to 1
  over the projected hemisphere; reflectance rises toward grazing.
- **M** · effect.

### 10 · 1981 · Depth of field (experiment 10)
M. Potmesil & I. Chakravarty, "A Lens and Aperture Camera Model for
Synthetic Image Generation", SIGGRAPH 1981. As specified in `EXPERIMENTS.md`.

### 17 · 1982 · Constructive solid geometry
S. D. Roth, "Ray Casting for Modeling Solids", Computer Graphics and Image
Processing 18(2), 1982.
- **Here:** union, intersection and difference of closed solids by merging
  the intervals each ray spends inside them (spheres of 11, closed meshes
  through the two-sided rays of 7b).
- **Proof:** the difference of two spheres gives the analytic bowl; a
  solid minus itself is empty; intervals stay ordered.
- **M** · modelling.

### 18 · 1982 · Implicit surfaces
J. Blinn, "A Generalization of Algebraic Surface Drawing", ACM TOG 1(3),
1982 (blobby molecules); P. Hanrahan, "Ray Tracing Algebraic Surfaces",
SIGGRAPH 1983.
- **Here:** metaballs and a torus, hit by bracketed root finding.
- **Proof:** a lone blob is the sphere of predicted radius; hit points
  satisfy f = T within 1e-4; the torus meets a ray at the quartic's roots.
- **M** · modelling.

### 19 · 1983 · Texture filtering
L. Williams, "Pyramidal Parametrics", SIGGRAPH 1983 (mip-maps);
J. Amanatides, "Ray Tracing with Cones", SIGGRAPH 1984.
- **Here:** mip-mapped textures, the level picked from a cone per pixel
  that widens with distance.
- **Proof:** a distant checker averages to its mean instead of aliasing;
  the level is log2 of the footprint in texels.
- **M** · image. Refined through mirrors in 49.

### 20 · 1984 · Distributed ray tracing, the rest ★
R. L. Cook, T. Porter & L. Carpenter, "Distributed Ray Tracing", SIGGRAPH
1984. Soft shadows were experiment 6, depth of field is 10.
- **Here:** glossy reflection and translucency (jittered rays in a cone
  around the mirror and refracted directions, a new sampler stream).
- **Proof:** the blur widens with roughness; roughness 0 is today's mirror
  bit for bit; the average over the cone conserves energy.
- **M** · effect.

### 21 · 1984 · Transforms and motion blur
Cook, Porter & Carpenter 1984, again: time as one more sampled dimension.
- **Here:** a transform per object (instancing, from the roadmap), rays
  moved into object space; a shutter interval and moving transforms.
- **Proof:** two instances give twice the hits of one; a quad moving by d
  during the shutter smears over exactly d.
- **M** · modelling.

### 22 · 1984 · Participating media ★
J. Kajiya & B. Von Herzen, "Ray Tracing Volume Densities", SIGGRAPH 1984.
- **Here:** homogeneous and gridded media (fog, smoke), single scattering by
  marching toward the lights, a phase function (isotropic, then
  Henyey-Greenstein).
- **Proof:** transmittance through a homogeneous slab is exp(-σt d) to 1e-4;
  an empty medium changes nothing.
- **M** · effect.

### 23 · 1984 · Spatial subdivision
A. Glassner, "Space Subdivision for Fast Ray Tracing", IEEE CG&A 4(10),
1984 (octrees); A. Fujimoto, T. Tanaka & K. Iwata, "ARTS: Accelerated
Ray-Tracing System", IEEE CG&A 6(4), 1986 (uniform grids); J. Amanatides &
A. Woo, "A Fast Voxel Traversal Algorithm for Ray Tracing", Eurographics
1987.
- **Here:** a uniform grid walked cell by cell (3D-DDA), next to the BVH.
- **Proof:** the random-ray suite of `TestBvh.cpp` gives the same hits;
  timings grid vs BVH on the teapot, the minion and the ground plane (where
  grids suffer).
- **M** · performance.

### 24 · 1984 · The Cornell box
C. Goral, K. Torrance, D. Greenberg & B. Battaile, "Modeling the Interaction
of Light Between Diffuse Surfaces", SIGGRAPH 1984.
- **Here:** radiosity itself is not ray tracing, but its scene is the test
  bench of everything that follows: the Cornell box (published geometry and
  reflectances) as a bundled model, with its ceiling light.
- **Proof:** loads with the published dimensions; renders in every mode.
- **S** · modelling.

**✓ 1984** · T. Porter & T. Duff, "Compositing Digital Images", SIGGRAPH
1984: coverage, the hit mask mode.

### 25 · 1985 · Solid noise
K. Perlin, "An Image Synthesizer", SIGGRAPH 1985.
- **Here:** gradient noise and turbulence as solid textures (marble, wood)
  for OFF models, which have no UVs, and as a bump source for 14.
- **Proof:** zero at lattice points, continuous, reproducible; its spectrum
  is band-limited.
- **S** · effect.

## Era II · Monte Carlo light transport (1986–1990)

**✓ 1986** · T. L. Kay & J. T. Kajiya, "Ray Tracing Complex Scenes",
SIGGRAPH 1986: the BVH, experiment 3. R. L. Cook, "Stochastic Sampling in
Computer Graphics", ACM TOG 5(1), 1986: jittered anti-aliasing, experiment 5.

### 26 · 1986 · Path tracing ★
J. T. Kajiya, "The Rendering Equation", SIGGRAPH 1986.
- **Here:** emissive materials, cosine-sampled diffuse bounces with
  next-event estimation toward the emitters, a `path` mode, progressive
  accumulation.
- **Proof:** the white furnace (a white diffuse object inside a uniform
  emitter converges to the emission); colour bleeding in the Cornell box;
  error falling as 1/sqrt(samples).
- **L** · effect. Needs 9 and 24; light sampling improves in 40, noise in
  38, 54, 66.

### 27 · 1986 · Backward ray tracing (caustics)
J. Arvo, "Backward Ray Tracing", SIGGRAPH 1986 course notes.
- **Here:** light tracing: paths from the lights, through the glass ram,
  splatted onto the image; the caustic under glass appears.
- **Proof:** a glass ball focuses a light at the ball-lens focal distance
  f = nR / (2(n - 1)).
- **M** · effect.

### 28 · 1986 · Dispersion
S. W. Thomas, "Dispersive Refraction in Ray Tracing", The Visual Computer
2(1), 1986.
- **Here:** an index that depends on wavelength (Cauchy's law), traced per
  wavelength band: a prism splits white light.
- **Proof:** blue deviates more than red by Cauchy's law; a constant index
  is today's picture bit for bit.
- **M** · effect. Noise-free with the spectral sampling of 73.

### 29 · 1987 · The surface area heuristic ★
J. Goldsmith & J. Salmon, "Automatic Creation of Object Hierarchies for Ray
Tracing", IEEE CG&A 7(5), 1987; J. D. MacDonald & K. S. Booth, "Heuristics
for Ray Tracing Using Space Subdivision", The Visual Computer 6(3), 1990.
- **Here:** SAH splits in the BVH builder, and the cost heatmap mode of the
  roadmap to see the difference (box and triangle tests per pixel).
- **Proof:** same hits; lower SAH cost and fewer tests per ray than the
  median split on every bundled model.
- **M** · performance.

### 30 · 1987 · Adaptive sampling and reconstruction filters
D. Mitchell, "Generating Antialiased Images at Low Sampling Densities",
SIGGRAPH 1987; D. Mitchell & A. Netravali, "Reconstruction Filters in
Computer Graphics", SIGGRAPH 1988.
- **Here:** more rays only where a pixel's samples disagree; Gaussian and
  Mitchell-Netravali filters instead of the box average.
- **Proof:** flat regions keep one sample; filter weights sum to 1; a step
  edge rings by the filter's known lobes.
- **M** · image.

### 31 · 1988 · Irradiance caching
G. Ward, F. Rubinstein & R. Clear, "A Ray Tracing Solution for Diffuse
Interreflection", SIGGRAPH 1988.
- **Here:** cache the diffuse bounce of 26 at sparse points and
  interpolate with the paper's error metric.
- **Proof:** within 2 % of uncached path tracing on the Cornell box, for a
  fraction of the rays.
- **M** · performance.

### 32 · 1989 · Distance estimation and sphere tracing
J. Hart, D. Sandin & L. Kauffman, "Ray Tracing Deterministic 3-D Fractals",
SIGGRAPH 1989; J. Hart, "Sphere Tracing", The Visual Computer 12(10), 1996.
- **Here:** signed distance fields marched by their own bound: SDF
  primitives and blends, and a quaternion Julia set.
- **Proof:** a sphere-traced SDF sphere hits where the analytic one does;
  the step count stays bounded; never a step past the surface.
- **M** · modelling.

### 33 · 1989 · Hair and fur
J. Kajiya & T. Kay, "Rendering Fur with Three Dimensional Textures",
SIGGRAPH 1989 (the Kajiya-Kay shading model); K. Nakamaru & Y. Ohno, "Ray
Tracing for Curves Primitive", WSCG 2002.
- **Here:** strand primitives (thin swept segments) with Kajiya-Kay shading:
  a woolly ram.
- **Proof:** the highlight follows the strand's tangent; a strand seen
  end-on covers the predicted width.
- **L** · modelling. Hardware-style strands in 92.

### 34 · 1990 · Russian roulette
J. Arvo & D. Kirk, "Particle Transport and Image Synthesis", SIGGRAPH 1990.
- **Here:** end paths at random with probability tied to their weight,
  reweighting the survivors; it also prunes the glass trees of 7b.
- **Proof:** the mean over seeds equals the fixed-depth reference within
  noise; rays per pixel drop.
- **S** · performance.

## Era III · Physically based rendering (1991–1999)

**1991, 1993** · Ward's HDR pixels and Tumblin & Rushmeier's tone
reproduction: in experiment 9.

### 35 · 1992 · Anisotropic reflection
G. Ward, "Measuring and Modeling Anisotropic Reflection", SIGGRAPH 1992.
- **Here:** the Ward BRDF: brushed metal, with a tangent frame per hit.
- **Proof:** isotropic when both roughnesses match; reciprocal; never
  reflects more than it receives.
- **S** · effect.

### 36 · 1993 · Bidirectional path tracing
E. Lafortune & Y. Willems, "Bi-directional Path Tracing", Compugraphics
1993; E. Veach & L. Guibas, "Bidirectional Estimators for Light
Transport", Eurographics Rendering Workshop 1994.
- **Here:** paths from the camera and from the lights, joined at every pair
  of vertices.
- **Proof:** converges to the same Cornell box as 26; caustics converge
  faster at equal time.
- **L** · effect.

### 37 · 1994 · Schlick's approximations
C. Schlick, "An Inexpensive BRDF Model for Physically-based Rendering",
Computer Graphics Forum 13(3), 1994.
- **Here:** Schlick's Fresnel as a cheap option for 16 and 60 (not for
  glass: it is not zero between equal indices, see 7b).
- **Proof:** within 1 % of the exact Fresnel for glass-like indices below
  80 degrees.
- **S** · effect.

### 38 · 1995 · Multiple importance sampling ★
E. Veach & L. Guibas, "Optimally Combining Sampling Techniques for Monte
Carlo Rendering", SIGGRAPH 1995.
- **Here:** light sampling and BSDF sampling combined by the balance and
  power heuristics; Veach's four glossy plates as a scene.
- **Proof:** the weights sum to 1 per path; on the plates, the MIS variance
  is at most that of the better strategy for every plate.
- **M** · sampling.

### 39 · 1996 · Photon mapping
H. W. Jensen, "Global Illumination Using Photon Maps", Eurographics
Rendering Workshop 1996.
- **Here:** global and caustic photon maps in a kd-tree, density estimation
  at the eye's hits.
- **Proof:** the caustic under the glass ball matches light tracing (27)
  as photons grow; bias shrinks with the gather radius.
- **L** · effect.

### 40 · 1996 · Area and mesh lights ★
P. Shirley, C. Wang & K. Zimmerman, "Monte Carlo Techniques for Direct
Lighting Calculations", ACM TOG 15(1), 1996.
- **Here:** any emissive mesh is a light, sampled by area and solid angle;
  the point-light rig becomes a special case.
- **Proof:** the irradiance under a disk light matches the analytic
  π L sin²θ; a tiny emitter behaves like today's point light.
- **M** · effect.

### 41 · 1996 · Displacement mapping
M. Pharr & P. Hanrahan, "Geometry Caching for Ray-Tracing Displacement
Maps", Eurographics Rendering Workshop 1996.
- **Here:** displaced geometry tessellated on demand and cached.
- **Proof:** a zero displacement is today's mesh; a constant one offsets the
  surface exactly along its normals.
- **L** · modelling. Hardware micro-meshes in 89.

### 42 · 1997 · Fast and watertight triangle tests ★
T. Möller & B. Trumbore, "Fast, Minimum Storage Ray-Triangle
Intersection", JGT 2(1), 1997; S. Woop, C. Benthin & I. Wald, "Watertight
Ray/Triangle Intersection", JCGT 2(1), 2013.
- **Here:** replace the 2010 plane-then-barycentrics test of `Ray::hit`.
- **Proof:** same hits on the random-ray suite; no ray slips between two
  triangles sharing an edge; speedup measured.
- **S** · performance.

### 43 · 1997 · Metropolis light transport
E. Veach & L. Guibas, "Metropolis Light Transport", SIGGRAPH 1997;
C. Kelemen, L. Szirmay-Kalos, G. Antal & F. Csonka, "A Simple and Robust
Mutation Strategy for the Metropolis Light Transport Algorithm", Computer
Graphics Forum 21(3), 2002.
- **Here:** primary-sample-space MLT on top of 26 or 36.
- **Proof:** converges to the same image; finds the light through a
  keyhole that path tracing misses at equal time.
- **L** · sampling.

### 44 · 1997 · Memory-coherent ray tracing
M. Pharr, C. Kolb, R. Gershbein & P. Hanrahan, "Rendering Complex Scenes
with Memory-Coherent Ray Tracing", SIGGRAPH 1997.
- **Here:** queue rays and trace them in batches grouped by the geometry
  they need, instead of depth first.
- **Proof:** same image; cache misses and time on the minion vs the
  depth-first tracer.
- **L** · performance, hardware. The ancestor of 67 and 89.

### 45 · 1997 · Instant radiosity
A. Keller, "Instant Radiosity", SIGGRAPH 1997.
- **Here:** virtual point lights shot from the emitters, then gathered like
  the rig's lights.
- **Proof:** converges to the path-traced Cornell box; the clamping bias is
  measured.
- **M** · effect.

### 46 · 1998 · Image-based lighting ★
P. Debevec, "Rendering Synthetic Objects into Real Scenes", SIGGRAPH 1998.
- **Here:** an HDR light probe (13, 9) lights the scene, importance-sampled
  by its luminance and combined by MIS (38).
- **Proof:** a uniform environment lights a white diffuse sphere to exactly
  its radiance; a single bright texel acts as a distant light.
- **M** · effect.

### 47 · 1998 · Volumetric photon mapping
H. W. Jensen & P. Christensen, "Efficient Simulation of Light Transport in
Scenes with Participating Media Using Photon Maps", SIGGRAPH 1998.
- **Here:** photons stored in media (22): light shafts, volume caustics.
- **Proof:** matches path-traced media (22 + 26) as photons grow.
- **L** · effect.

### 48 · 1999 · Interactive ray tracing
S. Parker, W. Martin, P.-P. Sloan, P. Shirley, B. Smits & C. Hansen,
"Interactive Ray Tracing", I3D 1999.
- **Here:** the other half after tile-parallel rendering (experiment 4): a
  progressive viewer that renders coarse first, refines, and restarts as the
  camera moves.
- **Proof:** each refinement pass converges to the full render byte for
  byte; the first image lands within a frame budget.
- **M** · real time.

### 49 · 1999 · Ray differentials
H. Igehy, "Tracing Ray Differentials", SIGGRAPH 1999.
- **Here:** carry the ray's footprint through mirrors and glass, so textures
  seen in reflections filter correctly (19).
- **Proof:** through a flat mirror the level picked equals the level at the
  virtual distance.
- **M** · image.

### 50 · 1999 · A physical sky
A. J. Preetham, P. Shirley & B. Smits, "A Practical Analytic Model for
Daylight", SIGGRAPH 1999; L. Hosek & A. Wilkie, "An Analytic Model for Full
Spectral Sky-Dome Radiance", SIGGRAPH 2012.
- **Here:** sun and sky from the turbidity and the sun's position, as the
  background and a light (46).
- **Proof:** zenith luminance and the sky gradient match the papers'
  formulas.
- **S** · effect.

## Era IV · Interactive and GPU ray tracing (2000–2009)

### 51 · 2001 · SIMD ray packets ★
I. Wald, P. Slusallek, C. Benthin & M. Wagner, "Interactive Rendering with
Coherent Ray Tracing", Computer Graphics Forum 20(3), 2001.
- **Here:** four rays at a time through the BVH with SIMD (NEON on the Mac,
  SSE on CI).
- **Proof:** packets return the same hits as single rays; primary-ray
  speedup measured.
- **L** · performance, hardware.

### 52 · 2001 · Subsurface scattering
H. W. Jensen, S. Marschner, M. Levoy & P. Hanrahan, "A Practical Model for
Subsurface Light Transport", SIGGRAPH 2001.
- **Here:** the dipole BSSRDF: a marble or jade ram.
- **Proof:** the dipole profile integrates to the paper's diffuse
  reflectance for the same albedo.
- **L** · effect.

### 53 · 2002 · Ray tracing on the GPU ★
T. Purcell, I. Buck, W. Mark & P. Hanrahan, "Ray Tracing on Programmable
Graphics Hardware", SIGGRAPH 2002.
- **Here:** a GPU compute backend (Metal on macOS; the CPU stays the
  reference and runs CI).
- **Proof:** the same image as the CPU within the golden tolerance, on
  every golden scene.
- **XL** · hardware.

**2002** · E. Reinhard, M. Stark, P. Shirley & J. Ferwerda, "Photographic
Tone Reproduction for Digital Images": experiment 9.

### 54 · 2002 · Quasi-Monte Carlo sampling ★
T. Kollig & A. Keller, "Efficient Multidimensional Sampling", Computer
Graphics Forum 21(3), 2002; B. Burley, "Practical Hash-based Owen
Scrambling", JCGT 9(4), 2020.
- **Here:** scrambled Sobol' points in every `Sampler` stream instead of
  jitter.
- **Proof:** on the analytic penumbra and occlusion tests the error decays
  faster than with jitter (log-log slope).
- **M** · sampling.

### 55 · 2005 · A ray processing unit
S. Woop, J. Schmittler & P. Slusallek, "RPU: A Programmable Ray Processing
Unit for Realtime Ray Tracing", SIGGRAPH 2005.
- **Here:** a software model of fixed-function traversal: quantised boxes,
  fixed-point tests, counters per pipeline stage, what hardware would save.
- **Proof:** the quantised boxes never lose a hit (conservative rounding);
  counts match the cost heatmap (29).
- **M** · hardware (study).

### 56 · 2005 · Resampled importance sampling
J. Talbot, D. Cline & P. Egbert, "Importance Resampling for Global
Illumination", Eurographics Symposium on Rendering 2005.
- **Here:** pick one light out of M candidates in proportion to its
  contribution, then weight it correctly.
- **Proof:** unbiased (the mean matches brute force); the variance drops as
  M grows on a scene of a thousand small lights.
- **M** · sampling. The root of ReSTIR (84).

### 57 · 2005 · Lightcuts
B. Walter, S. Fernandez, A. Arbree, K. Bala, M. Donikian & D. Greenberg,
"Lightcuts: A Scalable Approach to Illumination", SIGGRAPH 2005.
- **Here:** a tree over thousands of lights (45's VPLs), cut per pixel under
  an error bound.
- **Proof:** every pixel within the 2 % bound of brute force.
- **L** · performance.

### 58 · 2006 · SAH kd-trees
I. Wald & V. Havran, "On Building Fast kd-Trees for Ray Tracing, and on
Doing That in O(N log N)", IEEE Symposium on Interactive Ray Tracing 2006.
- **Here:** the kd-tree next to the grid (23) and the BVH (29).
- **Proof:** same hits; build and trace times on every bundled model.
- **M** · performance.

### 59 · 2007 · Deformable scenes
I. Wald, S. Boulos & P. Shirley, "Ray Tracing Deformable Scenes Using
Dynamic Bounding Volume Hierarchies", ACM TOG 26(1), 2007.
- **Here:** an animated ram: refit the BVH per frame, rebuild when it
  degrades.
- **Proof:** a refitted tree gives the same hits as a rebuilt one; its cost
  is tracked frame by frame.
- **M** · performance. Needs 21.

### 60 · 2007 · GGX and rough glass ★
B. Walter, S. Marschner, H. Li & K. Torrance, "Microfacet Models for
Refraction through Rough Surfaces", Eurographics Symposium on Rendering
2007.
- **Here:** the GGX distribution with Smith shadowing, in reflection and
  transmission: frosted glass.
- **Proof:** the distribution integrates to 1; roughness 0 reproduces the
  mirror and the glass of 7 and 7b; transmission conserves energy.
- **M** · effect.

### 61 · 2008 · Progressive photon mapping
T. Hachisuka, S. Ogaki & H. W. Jensen, "Progressive Photon Mapping",
SIGGRAPH Asia 2008; T. Hachisuka & H. W. Jensen, "Stochastic Progressive
Photon Mapping", SIGGRAPH Asia 2009.
- **Here:** 39 made consistent: shrinking radii over passes.
- **Proof:** the error on the caustic falls with every pass.
- **L** · effect.

### 62 · 2009 · Parallel BVH builds
C. Lauterbach, M. Garland, S. Sengupta, D. Luebke & D. Manocha, "Fast BVH
Construction on GPUs", Computer Graphics Forum 28(2), 2009.
- **Here:** Morton codes and a linear BVH built in parallel on every core.
- **Proof:** same hits; build time scales with threads (the minion).
- **M** · performance.

### 63 · 2009 · Spatial splits
M. Stich, H. Friedrich & A. Dietrich, "Spatial Splits in Bounding Volume
Hierarchies", High Performance Graphics 2009.
- **Here:** split triangles that span nodes (the ground plane's two huge
  triangles are the textbook case).
- **Proof:** same hits; fewer tests per ray than plain SAH.
- **M** · performance.

### 64 · 2009 · GPU traversal
T. Aila & S. Laine, "Understanding the Efficiency of Ray Traversal on
GPUs", High Performance Graphics 2009.
- **Here:** persistent threads and while-while loops in the GPU backend
  (53), measured against its first version.
- **Proof:** same image; rays per second on the paper's scene types.
- **M** · hardware.

## Era V · Production, denoising and ray tracing hardware (2010–2019)

### 65 · 2010 · A programmable ray tracing pipeline ★
S. Parker et al., "OptiX: A General Purpose Ray Tracing Engine", SIGGRAPH
2010.
- **Here:** split `RayTracer` into ray generation, intersection, any-hit,
  closest-hit and miss programs: the shape DXR and Metal later standardised,
  which makes 53 and 81 direct ports.
- **Proof:** every golden unchanged.
- **M** · architecture.

### 66 · 2010 · Edge-avoiding denoising ★
H. Dammertz, D. Sewtz, J. Hanika & H. Lensch, "Edge-Avoiding À-Trous
Wavelet Transform for Fast Global Illumination Filtering", High Performance
Graphics 2010.
- **Here:** an à-trous filter guided by normals, depth and albedo on the
  path-traced image.
- **Proof:** at 4 samples per pixel the result is closer to the converged
  image (PSNR); edges on normal discontinuities stay sharp.
- **M** · image.

### 67 · 2010 · Ray sorting
K. Garanzha & C. Loop, "Fast Ray Sorting and Breadth-First Packet Traversal
for GPU Ray Tracing", Computer Graphics Forum 29(2), 2010.
- **Here:** sort secondary rays by origin and direction before tracing
  (44's batches, finer).
- **Proof:** same image; coherence and time measured on the glass ram.
- **M** · performance.

### 68 · 2012 · Disney's principled BRDF
B. Burley, "Physically-Based Shading at Disney", SIGGRAPH 2012 course
(Physically Based Shading in Film and Game Production).
- **Here:** base colour, metallic, roughness, sheen, clearcoat: one
  artist-friendly material for the ram.
- **Proof:** never reflects more than it receives (white furnace); the
  metallic and roughness extremes match 16 and 60.
- **M** · effect.

### 69 · 2012 · Vertex connection and merging
I. Georgiev, J. Křivánek, T. Davidovič & P. Slusallek, "Light Transport
Simulation with Vertex Connection and Merging", SIGGRAPH Asia 2012;
T. Hachisuka, J. Pantaleoni & H. W. Jensen, "A Path Space Extension for
Robust Light Transport Simulation", SIGGRAPH Asia 2012.
- **Here:** bidirectional path tracing (36) and photon mapping (61) under one
  MIS.
- **Proof:** converges on scenes where each alone fails (caustics seen in a
  mirror).
- **XL** · effect.

### 70 · 2012 · Fast parallel BVH construction
T. Karras, "Maximizing Parallelism in the Construction of BVHs, Octrees,
and k-d Trees", HPG 2012; T. Karras & T. Aila, "Fast Parallel Construction
of High-Quality Bounding Volume Hierarchies", HPG 2013.
- **Here:** radix-tree builds, then treelet restructuring toward SAH
  quality.
- **Proof:** same hits; SAH cost within a few percent of 29 at a fraction of
  the build time.
- **M** · performance.

### 71 · 2014 · Embree
I. Wald, S. Woop, C. Benthin, G. Johnson & M. Ernst, "Embree: A Kernel
Framework for Efficient CPU Ray Tracing", SIGGRAPH 2014.
- **Here:** four- and eight-wide BVHs traversed with SIMD, and Embree
  itself as an optional backend to measure ourselves against.
- **Proof:** same hits; time per ray against Embree on every model.
- **M** · performance.

### 72 · 2014 · Microfacets that keep their energy
E. Heitz, "Understanding the Masking-Shadowing Function in Microfacet-Based
BRDFs", JCGT 3(2), 2014; E. Heitz, J. Hanika, E. d'Eon & C. Dachsbacher,
"Multiple-Scattering Microfacet BSDFs with the Smith Model", SIGGRAPH 2016;
C. Kulla & A. Conty, "Revisiting Physically Based Shading at Imageworks",
SIGGRAPH 2017 course.
- **Here:** the energy lost by single-scattering microfacets restored.
- **Proof:** a rough white metal sphere passes the white furnace (at least
  0.99 reflected).
- **M** · effect.

### 73 · 2014 · Spectral rendering
A. Wilkie, S. Nawaz, M. Droske, A. Weidlich & J. Hanika, "Hero Wavelength
Spectral Sampling", Computer Graphics Forum 33(4), 2014; W. Jakob &
J. Hanika, "A Low-Dimensional Function Space for Efficient Spectral
Upsampling", Computer Graphics Forum 38(2), 2019.
- **Here:** light carried as wavelengths, RGB materials lifted to spectra;
  dispersion (28) without colour noise.
- **Proof:** a flat spectrum renders today's RGB picture; the prism's
  spectrum lands where Cauchy says.
- **M** · effect.

### 74 · 2014 · Unbiased heterogeneous media
J. Novák, A. Selle & W. Jarosz, "Residual Ratio Tracking for Estimating
Attenuation in Participating Media", SIGGRAPH Asia 2014, after E. Woodcock
et al.'s delta tracking (1965).
- **Here:** smoke of any density field, traced without marching bias.
- **Proof:** transmittance estimates are unbiased against a finely
  integrated reference.
- **M** · effect.

### 75 · 2014 · Path guiding
J. Vorba, O. Karlík, M. Šik, T. Ritschel & J. Křivánek, "On-line Learning
of Parametric Mixture Models for Light Transport Simulation", SIGGRAPH
2014; T. Müller, M. Gross & J. Novák, "Practical Path Guiding for Efficient
Light-Transport Simulation", Computer Graphics Forum 36(4), 2017.
- **Here:** learn where light comes from (spatial-directional trees) and
  sample bounces there.
- **Proof:** lower error at equal time on a room lit through a door.
- **L** · sampling.

### 76 · 2016 · A scene description format
M. Pharr, W. Jakob & G. Humphreys, *Physically Based Rendering*, 3rd
edition, 2016 (pbrt).
- **Here:** read a subset of pbrt's scene format (shapes, materials,
  lights, camera): the roadmap's scene file, and access to reference scenes.
- **Proof:** a two-model scene loads with the expected objects and bounds;
  a pbrt scene renders comparably to pbrt's published image.
- **M** · modelling.

### 77 · 2017 · Spatiotemporal filtering in the viewer
C. Schied et al., "Spatiotemporal Variance-Guided Filtering", High
Performance Graphics 2017.
- **Here:** one path per pixel per frame while orbiting, accumulated over
  time and filtered by its variance (66).
- **Proof:** a static camera converges to the offline image; no ghosting
  after a cut.
- **L** · real time.

### 78 · 2017 · Compressed wide BVHs
H. Ylitie, T. Karras & S. Laine, "Efficient Incoherent Ray Traversal on
GPUs Through Compressed Wide BVHs", High Performance Graphics 2017.
- **Here:** eight-wide nodes with quantised child boxes (the layout RT
  hardware uses).
- **Proof:** same hits; bytes per triangle and time against the binary BVH.
- **M** · performance, hardware.

### 79 · 2017 · Learned denoisers
C. R. A. Chaitanya et al., "Interactive Reconstruction of Monte Carlo Image
Sequences Using a Recurrent Denoising Autoencoder", SIGGRAPH 2017; S. Bako
et al., "Kernel-Predicting Convolutional Networks for Denoising Monte
Carlo Renderings", SIGGRAPH 2017.
- **Here:** Intel Open Image Denoise as an optional dependency, against the
  à-trous filter of 66.
- **Proof:** PSNR against the converged image at 1, 4 and 16 samples.
- **M** · image.

### 80 · 2018 · Differentiable ray tracing
T.-M. Li, M. Aittala, F. Durand & J. Lehtinen, "Differentiable Monte Carlo
Ray Tracing through Edge Sampling", SIGGRAPH Asia 2018; M. Nimier-David,
D. Vicini, T. Zeltner & W. Jakob, "Mitsuba 2", SIGGRAPH Asia 2019.
- **Here:** gradients of the image with respect to a material colour and a
  light's position; recover them from a target picture of the ram by
  gradient descent.
- **Proof:** gradients match finite differences; the optimisation recovers
  the parameters used to make the target.
- **L** · inverse rendering.

### 81 · 2018 · Hardware ray tracing ★
Microsoft DirectX Raytracing (2018) and NVIDIA's Turing RT cores (Turing
architecture whitepaper, 2018); Apple added hardware ray tracing with the M3
(2023), under the Metal ray tracing API.
- **Here:** a Metal backend on `MTLAccelerationStructure`, so the Mac's RT
  units trace the rays (65's stages map onto it).
- **Proof:** the same image as the CPU within the golden tolerance; CPU,
  GPU compute (53) and RT hardware timed on the same scenes.
- **XL** · hardware.

### 82 · 2018 · Clustering builds
D. Meister & J. Bittner, "Parallel Locally-Ordered Clustering for Bounding
Volume Hierarchy Construction", IEEE TVCG 24(3), 2018.
- **Here:** PLOC builds, compared with 29, 62 and 70.
- **Proof:** same hits; SAH cost and build time on every model.
- **M** · performance.

### 83 · 2019 · Blue-noise error
E. Heitz & L. Belcour, "Distributing Monte Carlo Errors as a Blue Noise in
Screen Space by Permuting Pixel Seeds Between Frames", Computer Graphics
Forum 38(4), 2019; E. Heitz, L. Belcour, V. Ostromoukhov, D. Coeurjolly &
J.-C. Iehl, "A Low-Discrepancy Sampler that Distributes Monte Carlo Errors
as a Blue Noise in Screen Space", SIGGRAPH 2019 talks.
- **Here:** seeds per pixel chosen so the noise left is high-frequency (the
  eye and the denoiser both prefer it).
- **Proof:** the error image's power spectrum lacks low frequencies; the
  mean error is unchanged.
- **S** · sampling.

## Era VI · Real-time path tracing, neural and hybrid scenes (2020–2026)

### 84 · 2020 · ReSTIR ★
B. Bitterli, C. Wyman, M. Pharr, P. Shirley, A. Lefohn & W. Jarosz,
"Spatiotemporal Reservoir Resampling for Real-Time Ray Tracing with Dynamic
Direct Lighting", SIGGRAPH 2020.
- **Here:** reservoirs per pixel reused across neighbours and frames (on
  56): thousands of emissive triangles on the ram's scene.
- **Proof:** unbiased (the mean matches brute force); lower error than RIS
  alone at equal time.
- **L** · sampling.

### 85 · 2020 · Radiance fields
B. Mildenhall et al., "NeRF: Representing Scenes as Neural Radiance Fields
for View Synthesis", ECCV 2020; S. Fridovich-Keil et al., "Plenoxels:
Radiance Fields without Neural Networks", CVPR 2022.
- **Here:** volume-render a pretrained voxel radiance field (Plenoxels need
  no network) by the same quadrature as 22, and put the ram in it.
- **Proof:** the renderer reproduces the published test views within their
  PSNR.
- **L** · modelling.

### 86 · 2021 · ReSTIR for indirect light and whole paths
Y. Ouyang, S. Liu, M. Kettunen, M. Pharr & J. Pantaleoni, "ReSTIR GI: Path
Resampling for Real-Time Path Tracing", HPG 2021; D. Lin, M. Kettunen,
B. Bitterli, J. Pantaleoni, C. Yuksel & C. Wyman, "Generalized Resampled
Importance Sampling: Foundations of ReSTIR", SIGGRAPH 2022.
- **Here:** resample whole paths with shift mappings.
- **Proof:** unbiased against the path-traced reference; error at equal
  time.
- **XL** · sampling.

### 87 · 2021 · Neural radiance caching
T. Müller, F. Rousselle, J. Novák & A. Keller, "Real-time Neural Radiance
Caching for Path Tracing", SIGGRAPH 2021.
- **Here:** a small MLP, written by hand for the CPU, trained online to
  cache radiance and end paths early.
- **Proof:** the cache's bias measured against the reference; time saved
  per sample.
- **XL** · neural.

### 88 · 2022 · Multiresolution hash encoding
T. Müller, A. Evans, C. Schied & A. Keller, "Instant Neural Graphics
Primitives with a Multiresolution Hash Encoding", SIGGRAPH 2022.
- **Here:** the hash grid as the input encoding of 87, or as a learned SDF
  (32) of a model.
- **Proof:** a fitted SDF matches the mesh's distance within a voxel.
- **L** · neural, modelling.

### 89 · 2022 · Execution reordering and micro-geometry
NVIDIA Ada Lovelace architecture whitepaper (2022): shader execution
reordering, opacity micromaps, displaced micro-meshes.
- **Here:** CPU counterparts: shade hits regrouped by material (after 44 and
  67), micromaps for alpha-cut leaves, micro-meshes for 41.
- **Proof:** same image; the any-hit calls saved by the micromaps counted.
- **M** · hardware.

### 90 · 2023 · Ray traced Gaussians ★
B. Kerbl, G. Kopanas, T. Leimkühler & G. Drettakis, "3D Gaussian Splatting
for Real-Time Radiance Field Rendering", SIGGRAPH 2023; N. Moenne-Loccoz,
A. Mirzaei, O. Perel, R. de Lutio, J. Martinez Esturo, G. State, S. Fidler,
N. Sharp & Z. Gojcic, "3D Gaussian Ray Tracing: Fast Tracing of Particle
Scenes", SIGGRAPH Asia 2024.
- **Here:** load a pretrained Gaussian scene (`.ply`), bound each particle
  by a small mesh in the BVH, composite hits in depth order; the ram stands
  in a captured scene, with shadows and reflections between the two.
- **Proof:** the published test views reproduced within their PSNR; a lone
  Gaussian's opacity profile along a ray is the analytic one.
- **L** · modelling.

### 91 · 2025 · Radiant foam
S. Govindarajan, D. Rebain, K. M. Yi & A. Tagliasacchi, "Radiant Foam:
Real-Time Differentiable Ray Tracing", ICCV 2025.
- **Here:** a radiance field stored in Voronoi cells, traced by walking from
  cell to neighbouring cell, no hardware needed.
- **Proof:** the walk visits exactly the cells a brute-force test finds;
  published views reproduced.
- **L** · modelling.

### 92 · 2025 · Cluster geometry and swept spheres
NVIDIA RTX Blackwell architecture (2025): cluster-level acceleration
structures ("RTX Mega Geometry", with a cluster intersection engine in the
RT cores) and linear swept spheres, a hardware primitive for hair.
- **Here:** a two-level BVH over clusters of about 128 triangles, rebuilt
  per cluster; linear swept spheres for the strands of 33.
- **Proof:** same hits as the flat BVH; memory and rebuild time on the
  minion; a swept sphere matches its capsule analytically.
- **M** · hardware, modelling.

### 93 · 2025 · Guiding with resampled paths
Z. Zeng, M. Kettunen, C. Wyman, L. Wu, R. Ramamoorthi, L.-Q. Yan & D. Lin,
"ReSTIR PG: Path Guiding with Spatiotemporally Resampled Paths", SIGGRAPH
Asia 2025.
- **Here:** 75's guiding trained from the paths 86 resamples.
- **Proof:** lower error at equal time than 75 and 86 alone.
- **L** · sampling.

### 94 · 2026 · Where Gaussian ray tracing stands now
"Stochastic Ray Tracing of Transparent 3D Gaussians" (arXiv 2504.06598,
2025), "Stochastic Ray Tracing for the Reconstruction of 3D Gaussian
Splatting" (arXiv 2603.23637, March 2026), "GRay: Ray Tracing 3D Gaussians
Near the Speed of Splats" (arXiv 2606.30869, June 2026).
- **Here:** revisit 90 with stochastic transparency (one sampled hit per
  ray instead of sorted compositing) and measure the gap to rasterisation.
- **Proof:** converges to 90's image; time per frame against it.
- **M** · modelling, performance.

## A route through it

The backbone (★), in order: 9, 11, 12, 13, 16, 10, 20, 22, 26, 29, 38, 40,
42, 46, 51, 53, 54, 60, 65, 66, 81, 84, 90. It takes raymini from Whitted
to a path tracer with textures, glossy and rough materials, area and
environment lights, a denoiser, a GPU and RT-hardware backend, ReSTIR and
Gaussian scenes. The other steps branch off it and can be taken in any
order once their dependencies are there.

Rough size: 84 steps, 11 S, 47 M, 21 L and 5 XL. At the pace of the
first ten experiments, the backbone alone is a long project; the whole list
is a curriculum.
