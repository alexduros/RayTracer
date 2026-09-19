// *********************************************************
// Ray Tracer Class
// Author : Tamy Boubekeur (boubek@gmail.com).
// Copyright (C) 2010 Tamy Boubekeur.
// All rights reserved.
// *********************************************************

#include "RayTracer.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>

namespace {

inline unsigned char toByte (float c) {
    int v = static_cast<int> (c * 255.f + 0.5f);
    return static_cast<unsigned char> (std::max (0, std::min (255, v)));
}

/// How far secondary rays (shadows, reflections) start off the surface along
/// the normal, so the surface cannot hit itself ("acne"); scaled to the model.
inline float surfaceBias (const Scene & scene) {
    return 1e-4f * std::max (scene.getBoundingBox ().getSize (), 1e-3f);
}

/// HSV (all in [0,1]) to linear RGB.
Vec3Df hsvToRgb (float h, float s, float v) {
    const float hh = (h - std::floor (h)) * 6.f;
    const int sector = static_cast<int> (hh);
    const float f = hh - static_cast<float> (sector);
    const float p = v * (1.f - s), q = v * (1.f - s * f), t = v * (1.f - s * (1.f - f));
    switch (sector) {
        case 0: return Vec3Df (v, t, p);
        case 1: return Vec3Df (q, v, p);
        case 2: return Vec3Df (p, v, t);
        case 3: return Vec3Df (p, q, v);
        case 4: return Vec3Df (t, p, v);
        default: return Vec3Df (v, p, q);
    }
}

/// Maps [0,1]^2 onto the unit disk so that equal areas stay equal and
/// neighbouring points stay neighbours: jittered grid cells land on the disk
/// as compact patches (P. Shirley & K. Chiu's concentric map).
void concentricDisk (float a, float b, float & x, float & y) {
    const float sx = 2.f * a - 1.f, sy = 2.f * b - 1.f;
    if (sx == 0.f && sy == 0.f) {
        x = y = 0.f;
        return;
    }
    const float quarterPi = 0.78539816f;
    float r, theta;
    if (std::fabs (sx) > std::fabs (sy)) {
        r = sx;
        theta = quarterPi * (sy / sx);
    } else {
        r = sy;
        theta = 2.f * quarterPi - quarterPi * (sx / sy);
    }
    x = r * std::cos (theta);
    y = r * std::sin (theta);
}

const RayTracer::ModeInfo kModeInfos[RayTracer::kModeCount] = {
    {"lit", "Lit (Lambert + Blinn-Phong, shadows)",
     "For each light: material colour x light colour x max(0, n.l), the cosine between the normal and the light "
     "direction (Lambert); plus a white highlight where the half-vector between the light and view directions "
     "lines up with the normal, raised to the shininess (Blinn-Phong); both scaled by the fraction of the light "
     "that shadow rays find unblocked (one ray: all or nothing; soft shadows: a grid of rays over the light's "
     "disk); plus a constant ambient term. With ambient occlusion the ambient and diffuse terms are scaled by how "
     "open the surroundings are (see the AO mode). On a reflective material, blended with what the mirrored ray "
     "sees.",
     "Brighter where a surface faces a light; tight bright spots are highlights; where a light is blocked only "
     "the ambient term and the other lights remain, and with soft shadows the edge fades across a penumbra. "
     "Colour is material x light, so the cyan key light tints the orange default material green. Turn on the "
     "ground plane to see the shadows fall, and give it some reflectivity to see the model mirrored in it.",
     "J. H. Lambert, Photometria (1760); J. Blinn, \"Models of Light Reflection for Computer Synthesized "
     "Pictures\", SIGGRAPH 1977; shadow rays: A. Appel, AFIPS 1968 and T. Whitted, CACM 23(6), 1980."},
    {"ambient", "Ambient (albedo)",
     "The material's base colour (Kd) at the hit point with no lighting at all: what the surface would reflect "
     "under uniform white light.",
     "Flat silhouettes in each material's colour. Use it to check materials and object outlines; shape is "
     "invisible because nothing is shaded.",
     "The ambient term of B. T. Phong, \"Illumination for Computer Generated Pictures\", Communications of the "
     "ACM 18(6), 1975."},
    {"hitmask", "Hit mask (coverage)",
     "White where the primary ray hits geometry, black where it escapes to the background: the coverage (alpha) "
     "of the scene in each pixel.",
     "A binary silhouette at one ray per pixel; with anti-aliasing, edge pixels turn grey in proportion to how "
     "much of the pixel the object covers.",
     "T. Porter & T. Duff, \"Compositing Digital Images\", SIGGRAPH 1984: alpha as pixel coverage."},
    {"normals", "Normals",
     "The interpolated surface normal at the hit, remapped from [-1, 1] to [0, 1] per axis: x to red, y to "
     "green, z to blue.",
     "A face pointing at the camera (+z) is light violet (0.5, 0.5, 1); one pointing up (+y) is light green. "
     "Smooth gradients mean smooth shading normals, flat patches mean hard edges (as on cube.obj).",
     "The encoding used by normal maps: J. Cohen, M. Olano & D. Manocha, \"Appearance-Preserving "
     "Simplification\", SIGGRAPH 1998; perturbing normals goes back to J. Blinn, \"Simulation of Wrinkled "
     "Surfaces\", SIGGRAPH 1978."},
    {"depth", "Depth",
     "Distance from the eye to the hit, mapped linearly between the near and far settings: white at near, dark "
     "grey at far and beyond, black where nothing is hit.",
     "Brighter is closer. If everything is one shade, tighten near/far around the model (the defaults bracket "
     "its bounding box).",
     "The z-buffer: E. Catmull, \"A Subdivision Algorithm for Computer Display of Curved Surfaces\", PhD thesis, "
     "University of Utah, 1974."},
    {"objectid", "Object id",
     "One flat colour per object from a fixed palette indexed by object number. OBJ files give one object per "
     "material, so this shows the material groups; an OFF file is a single object.",
     "Same colour = same object. A one-colour picture on an OFF model is expected, not a bug.",
     "The item buffer: H. Weghorst, G. Hooper & D. P. Greenberg, \"Improved Computational Methods for Ray "
     "Tracing\", ACM Transactions on Graphics 3(1), 1984."},
    {"ao", "Ambient occlusion",
     "n x n rays leave the hit point over the hemisphere around the normal, cosine-weighted (one jittered ray per "
     "cell of a grid on the unit disk, lifted onto the hemisphere); the share that meets nothing within the radius "
     "measures how open the surroundings are. This mode shows that share as grey; in Lit mode it scales the "
     "ambient and diffuse terms (not the highlight).",
     "White is open, darker is enclosed: creases, the inside of the horns, the floor around the feet and under "
     "the belly. The radius sets the reach: small, only contact points darken; large, whole cavities. Few samples "
     "leave grain. Needs occlusion on (--ao n; the CLI takes 8x8 when --mode ao comes alone), otherwise every hit "
     "is white. Darkening the direct light too is an approximation that helps shapes read, as in the 2013 "
     "version, which darkened the whole colour.",
     "S. Zhukov, A. Iones & G. Kronin, \"An Ambient Light Illumination Model\", Eurographics Rendering Workshop "
     "1998; cosine-weighted directions by Malley's method, on P. Shirley & K. Chiu's concentric map (JGT 2(3), "
     "1997)."},
};

const RayTracer::ModeInfo kAntiAliasingInfo = {
    "aa", "Anti-aliasing (supersampling)",
    "Several primary rays per pixel on an n x n sub-pixel grid, averaged in linear colour. Jitter offsets each "
    "ray randomly inside its cell with a per-pixel seed, so renders stay reproducible.",
    "One ray per pixel gives staircase edges; 2x2 or 3x3 smooths them at 4x or 9x the cost. Jitter trades the "
    "remaining regular pattern for fine noise.",
    "T. Whitted, \"An Improved Illumination Model for Shaded Display\", Communications of the ACM 23(6), 1980 "
    "(supersampling in ray tracing); R. L. Cook, \"Stochastic Sampling in Computer Graphics\", ACM Transactions "
    "on Graphics 5(1), 1986 (jittered sampling)."};

const RayTracer::ModeInfo kSoftShadowsInfo = {
    "softshadows", "Soft shadows (area lights)",
    "Each light is a disk instead of a point: n x n shadow rays toward points spread over it (one jittered ray per "
    "cell of a grid mapped onto the disk) measure the fraction of the light a surface point sees, and the light's "
    "contribution is scaled by that fraction.",
    "Shadows gain a penumbra, a gradient from full shadow (umbra) to full light, wider for bigger lights and for "
    "occluders farther from the surface: contact shadows stay sharp. Few samples leave grain in the penumbra; each "
    "light costs n x n shadow rays per shaded point.",
    "R. L. Cook, T. Porter & L. Carpenter, \"Distributed Ray Tracing\", SIGGRAPH 1984; P. Shirley & K. Chiu, \"A Low "
    "Distortion Map Between Disk and Square\", Journal of Graphics Tools 2(3), 1997 (concentric map)."};

const RayTracer::ModeInfo kReflectionsInfo = {
    "reflections", "Mirror reflections",
    "On a reflective material a second ray leaves the hit point, mirrored about the normal (angle out = angle in), "
    "and what it sees is blended in: colour = (1 - k) x the surface's own shading + k x the reflected colour, k "
    "being the material's reflectivity. The reflected ray is shaded the same way, recursively, up to the maximum "
    "depth; at the limit a surface keeps its own shading.",
    "A mirror floor shows the model upside down under it; reflections of reflections appear between facing "
    "mirrors, each one fainter by k. Where the mirrored ray escapes, the surface darkens toward the background. "
    "Mirrors are perfect: sharp and uncoloured, so the highlight of a point light fades with k (the light itself "
    "is not an object a ray can meet). Depth 0 turns reflections off.",
    "T. Whitted, \"An Improved Illumination Model for Shaded Display\", Communications of the ACM 23(6), 1980 "
    "(recursive ray tracing)."};

} // namespace

const RayTracer::ModeInfo & RayTracer::info (DebugMode mode) {
    const int i = static_cast<int> (mode);
    return kModeInfos[(i >= 0 && i < kModeCount) ? i : 0];
}

const RayTracer::ModeInfo & RayTracer::antiAliasingInfo () {
    return kAntiAliasingInfo;
}

const RayTracer::ModeInfo & RayTracer::softShadowsInfo () {
    return kSoftShadowsInfo;
}

const RayTracer::ModeInfo & RayTracer::reflectionsInfo () {
    return kReflectionsInfo;
}

namespace {

// The roadmap, one sentence of principle each. Order = suggested order of
// implementation within each family; the document groups them.
const RayTracer::ModeInfo kPlannedModes[RayTracer::kPlannedModeCount] = {
    {"refraction", "Refraction (glass)",
     "Rays bend through transparent surfaces following Snell's law and split between reflection and "
     "transmission by the Fresnel term.",
     "Builds on the mirror reflections; needs an index of refraction (MTL Ni) and transparency (MTL d).",
     "T. Whitted, CACM 23(6), 1980; C. Schlick, \"An Inexpensive BRDF Model for Physically-based Rendering\", "
     "Computer Graphics Forum 13(3), 1994."},
    {"pathtracing", "Path tracing (global illumination)",
     "Each pixel averages many random light paths bouncing through the scene, converging on the rendering "
     "equation with indirect light and colour bleeding.",
     "Needs emissive materials and many samples per pixel; the BVH and the progressive display already exist.",
     "J. T. Kajiya, \"The Rendering Equation\", SIGGRAPH 1986."},
    {"environment", "Environment lighting (HDR sky)",
     "A panoramic image lights the scene: rays that miss geometry return the sky's colour and diffuse surfaces "
     "integrate it over the hemisphere.",
     "Needs HDR image loading (stb reads .hdr) and hemisphere sampling.",
     "P. Debevec, \"Rendering Synthetic Objects into Real Scenes\", SIGGRAPH 1998."},
    {"textures", "Textures (procedural and image)",
     "The material colour becomes a function of the hit: a checker or noise of the position, or a bitmap looked "
     "up through interpolated texture coordinates.",
     "Needs UVs kept from OBJ files and MTL map_Kd; procedural patterns first, since OFF models have no UVs.",
     "E. Catmull, PhD thesis, University of Utah, 1974 (texture mapping); K. Perlin, \"An Image Synthesizer\", "
     "SIGGRAPH 1985 (noise)."},
    {"pbr", "Physically based materials (GGX)",
     "A microfacet model shapes the highlight from a statistical distribution of tiny mirrors with Fresnel and "
     "masking terms, driven by roughness and metalness.",
     "Needs roughness and metalness on Material; supersedes Blinn-Phong.",
     "R. Cook & K. Torrance, \"A Reflectance Model for Computer Graphics\", SIGGRAPH 1981; B. Walter et al., "
     "\"Microfacet Models for Refraction through Rough Surfaces\", EGSR 2007."},
    {"dof", "Depth of field",
     "Rays start from random points on a lens disk and converge on the focal plane, blurring whatever lies "
     "nearer or farther.",
     "Needs an aperture and a focus distance on Camera plus per-pixel sampling. Experiment 10.",
     "M. Potmesil & I. Chakravarty, \"A Lens and Aperture Camera Model for Synthetic Image Generation\", SIGGRAPH "
     "1981; Cook, Porter & Carpenter, SIGGRAPH 1984."},
    {"tonemap", "Tone mapping and exposure",
     "Radiance stays linear in floats and is mapped to the display range by an exposure and a tone curve before "
     "sRGB encoding, so bright scenes no longer clip.",
     "Needs a floating-point image buffer. Experiment 9.",
     "E. Reinhard, M. Stark, P. Shirley & J. Ferwerda, \"Photographic Tone Reproduction for Digital Images\", "
     "SIGGRAPH 2002."},
    {"wireframe", "Wireframe overlay",
     "Pixels whose barycentric coordinates lie close to a triangle edge are drawn dark over the shaded image, "
     "showing the tessellation of the model.",
     "Needs the barycentric coordinates Ray::hit already computes but does not return.",
     "J. A. Baerentzen, S. L. Nielsen, M. Gjoel, B. D. Larsen & N. J. Christensen, \"Single-pass Wireframe "
     "Rendering\", SIGGRAPH 2006 Sketches."},
    {"cost", "Cost heatmap",
     "Each pixel is coloured by how many triangle or bounding-box tests its rays needed, showing where the time "
     "goes on complex models.",
     "Needs box and triangle test counters in closestHit and the BVH traversal; shows what the BVH saves and "
     "where it cannot.",
     "I. Wald, \"Realtime Ray Tracing and Interactive Global Illumination\", PhD thesis, Saarland University, "
     "2004."},
};

} // namespace

const RayTracer::ModeInfo & RayTracer::plannedMode (int index) {
    return kPlannedModes[(index >= 0 && index < kPlannedModeCount) ? index : 0];
}

bool RayTracer::closestHit (const Scene & scene, const Ray & ray, Hit & best) const {
    bool found = false;
    best.distance = std::numeric_limits<float>::max ();
    best.backFace = false;
    const std::vector<Object> & objects = scene.getObjects ();
    for (unsigned int i = 0; i < objects.size (); ++i) {
        const Object & object = objects[i];
        Vertex v;
        float t = best.distance;  // the BVH only looks for hits closer than this
        unsigned int triangle = 0;
        const bool hit = bvhEnabled ? object.getBvh ().nearestHit (ray, object.getMesh (), v, t, triangle)
                                    : ray.nearestHit (object.getMesh (), v, t, triangle);
        if (hit && t < best.distance) {
            best.distance = t;
            best.vertex = v;
            best.objectIndex = i;
            best.triangleIndex = triangle;
            found = true;
        }
    }
    if (found && ray.isTwoSided ()) {
        // Which side, from the triangle's own (geometric) normal: the
        // interpolated one can disagree near a silhouette.
        const Mesh & mesh = objects[best.objectIndex].getMesh ();
        const Triangle & triangle = mesh.getTriangles ()[best.triangleIndex];
        const Vec3Df & a = mesh.getVertices ()[triangle.getVertex (0)].getPos ();
        const Vec3Df & b = mesh.getVertices ()[triangle.getVertex (1)].getPos ();
        const Vec3Df & c = mesh.getVertices ()[triangle.getVertex (2)].getPos ();
        best.backFace = Vec3Df::dotProduct (Vec3Df::crossProduct (b - a, c - a), ray.getDirection ()) > 0.f;
    }
    return found;
}

bool RayTracer::occluded (const Scene & scene, const Ray & ray, float maxDistance) const {
    Vertex v;
    float t = 0.f;
    for (const Object & object : scene.getObjects ()) {
        if (bvhEnabled) {
            if (object.getBvh ().anyHit (ray, object.getMesh (), maxDistance))
                return true;
            continue;
        }
        for (const Triangle & triangle : object.getMesh ().getTriangles ())
            if (ray.hit (triangle, object.getMesh (), v, t) && t < maxDistance)
                return true;
    }
    return false;
}

float RayTracer::lightVisibility (const Scene & scene, const Vec3Df & p, const Vec3Df & n, const Light & light,
                                  Sampler & sampler) const {
    const Vec3Df origin = p + n * surfaceBias (scene);
    Vec3Df l = light.getPos () - p;
    const float distanceToLight = l.normalize ();

    const unsigned int count = shadowSamplesPerAxis;
    if (count <= 1 || light.getRadius () <= 0.f)
        return occluded (scene, Ray (origin, l), distanceToLight) ? 0.f : 1.f;  // a point: all or nothing

    // A disk of the light's radius facing the point, split into count x count
    // cells with one jittered sample each. Both numbers of a cell are drawn
    // whatever happens to its ray, so the sequence never depends on the scene.
    Vec3Df u, w;
    l.getTwoOrthogonals (u, w);
    u.normalize ();
    w.normalize ();
    unsigned int unblocked = 0;
    for (unsigned int j = 0; j < count; ++j) {
        for (unsigned int i = 0; i < count; ++i) {
            const float a = (static_cast<float> (i) + sampler.next ()) / static_cast<float> (count);
            const float b = (static_cast<float> (j) + sampler.next ()) / static_cast<float> (count);
            float dx = 0.f, dy = 0.f;
            concentricDisk (a, b, dx, dy);
            Vec3Df d = light.getPos () + light.getRadius () * (dx * u + dy * w) - origin;
            const float distance = d.normalize ();
            // The part of the disk below the surface's horizon is hidden.
            if (Vec3Df::dotProduct (d, n) > 0.f && !occluded (scene, Ray (origin, d), distance))
                ++unblocked;
        }
    }
    return static_cast<float> (unblocked) / static_cast<float> (count * count);
}

float RayTracer::ambientOcclusion (const Scene & scene, const Vec3Df & p, const Vec3Df & n, Sampler & sampler) const {
    const unsigned int count = aoSamplesPerAxis;
    if (count == 0 || aoRadius <= 0.f)
        return 1.f;
    // Cosine-weighted hemisphere (Malley): a jittered point per cell of a
    // count x count grid mapped onto the unit disk, lifted straight up onto
    // the hemisphere. Each ray then counts equally. Both numbers of a cell
    // are drawn whatever happens to its ray.
    Vec3Df u, w;
    n.getTwoOrthogonals (u, w);
    u.normalize ();
    w.normalize ();
    const Vec3Df origin = p + n * surfaceBias (scene);
    unsigned int open = 0;
    for (unsigned int j = 0; j < count; ++j) {
        for (unsigned int i = 0; i < count; ++i) {
            const float a = (static_cast<float> (i) + sampler.next ()) / static_cast<float> (count);
            const float b = (static_cast<float> (j) + sampler.next ()) / static_cast<float> (count);
            float dx = 0.f, dy = 0.f;
            concentricDisk (a, b, dx, dy);
            const float dz = std::sqrt (std::max (0.f, 1.f - dx * dx - dy * dy));
            Vec3Df d = dx * u + dy * w + dz * n;
            d.normalize ();
            if (!occluded (scene, Ray (origin, d), aoRadius))
                ++open;
        }
    }
    return static_cast<float> (open) / static_cast<float> (count * count);
}

Vec3Df RayTracer::shade (const Scene & scene, const Ray & ray, const Hit & hit, PixelSamplers & samplers,
                         unsigned int depth) const {
    const Material & mat = scene.getObjects ()[hit.objectIndex].getMaterial ();
    switch (debugMode) {
        case DebugMode::HIT_MASK:
            return Vec3Df (1.f, 1.f, 1.f);
        case DebugMode::NORMALS: {
            Vec3Df n = hit.vertex.getNormal ();
            n.normalize ();
            return 0.5f * (n + Vec3Df (1.f, 1.f, 1.f));
        }
        case DebugMode::DEPTH: {
            // z-buffer look: white at near, dark grey at far (never black, so
            // a far surface still reads against the black background).
            const float range = depthFar - depthNear;
            const float t = range > 0.f ? std::clamp ((hit.distance - depthNear) / range, 0.f, 1.f) : 0.f;
            const float v = 1.f - 0.8f * t;
            return Vec3Df (v, v, v);
        }
        case DebugMode::OBJECT_ID: {
            // Golden-ratio hue steps: neighbouring indices get clearly
            // different, equally saturated colours.
            const float hue = static_cast<float> (hit.objectIndex) * 0.61803398875f;
            return hsvToRgb (hue, 0.6f, 0.95f);
        }
        case DebugMode::AMBIENT:
            return mat.getColor ();
        case DebugMode::AMBIENT_OCCLUSION: {
            Vec3Df n = hit.vertex.getNormal ();
            n.normalize ();
            const float open = ambientOcclusion (scene, hit.vertex.getPos (), n, samplers.occlusion);
            return Vec3Df (open, open, open);
        }
        case DebugMode::LIT:
        default: {
            // Lambert diffuse + Blinn-Phong highlight, each light scaled by
            // how much of it the point sees (hard or soft shadows), ambient
            // and diffuse by how open its surroundings are (occlusion), then
            // the mirror reflection on reflective materials. No attenuation.
            Vec3Df n = hit.vertex.getNormal ();
            n.normalize ();
            const Vec3Df & p = hit.vertex.getPos ();
            Vec3Df v = -ray.getDirection ();
            v.normalize ();

            // 1 when occlusion is off: every product below is then unchanged.
            const float open = ambientOcclusion (scene, p, n, samplers.occlusion);
            Vec3Df color = (open * ambientIntensity) * mat.getColor ();
            for (const Light & light : scene.getLights ()) {
                Vec3Df l = light.getPos () - p;
                l.normalize ();
                const float nDotL = Vec3Df::dotProduct (n, l);
                if (nDotL <= 0.f)
                    continue;  // light behind the surface
                const float visibility = shadows ? lightVisibility (scene, p, n, light, samplers.shadows) : 1.f;
                if (visibility <= 0.f)
                    continue;  // the whole light is blocked
                color += (open * visibility * mat.getDiffuse () * light.getIntensity () * nDotL) *
                         (mat.getColor () * light.getColor ());
                if (specularEnabled && mat.getSpecular () > 0.f) {
                    Vec3Df h = l + v;
                    h.normalize ();
                    const float nDotH = std::max (0.f, Vec3Df::dotProduct (n, h));
                    color += (visibility * mat.getSpecular () * light.getIntensity () *
                              std::pow (nDotH, mat.getShininess ())) *
                             light.getColor ();
                }
            }

            const float k = mat.getReflectivity ();
            if (k <= 0.f || depth >= maxDepth)
                return color;
            // Mirror the view ray about the normal turned toward it (a smooth
            // normal can face away at a silhouette), start it off the surface
            // like a shadow ray, and blend in what it sees.
            Vec3Df d = ray.getDirection ();
            d.normalize ();
            if (Vec3Df::dotProduct (d, n) > 0.f)
                n = -n;
            Vec3Df r = d - (2.f * Vec3Df::dotProduct (d, n)) * n;
            r.normalize ();
            const Ray reflected (p + n * surfaceBias (scene), r);
            Hit next;
            const Vec3Df seen = closestHit (scene, reflected, next)
                                    ? shade (scene, reflected, next, samplers, depth + 1)
                                    : backgroundColor;
            return (1.f - k) * color + k * seen;
        }
    }
}

Vec3Df RayTracer::trace (const Scene & scene, const Ray & ray, Stats & stats) const {
    PixelSamplers samplers;
    return trace (scene, ray, stats, samplers);
}

Vec3Df RayTracer::trace (const Scene & scene, const Ray & ray, Stats & stats, PixelSamplers & samplers) const {
    stats.rays++;
    Hit hit;
    if (!closestHit (scene, ray, hit))
        return backgroundColor;
    stats.hits++;
    if (stats.hits == 1 || hit.distance < stats.minHitDist)
        stats.minHitDist = hit.distance;
    if (hit.distance > stats.maxHitDist)
        stats.maxHitDist = hit.distance;
    return shade (scene, ray, hit, samplers);
}

void RayTracer::renderRegion (const Scene & scene, const Camera & camera,
                              unsigned int width, unsigned int height,
                              unsigned int x0, unsigned int y0, unsigned int x1, unsigned int y1,
                              Image & image, Stats & stats) const {
    const unsigned int n = std::max (1u, aaSamplesPerAxis);
    for (unsigned int y = y0; y < y1; y++) {
        for (unsigned int x = x0; x < x1; x++) {
            // Seeded by the pixel: the same picture whatever the tile order.
            PixelSamplers samplers = PixelSamplers::forPixel (x, y);
            Vec3Df color;
            if (n == 1) {
                // One ray through the pixel centre.
                color = trace (scene, camera.primaryRay (x, y, width, height), stats, samplers);
            } else {
                // n x n sub-pixel grid, optionally jittered inside each cell.
                Sampler jitter = Sampler::forPixel (x, y, Sampler::Stream::PIXEL_JITTER);
                for (unsigned int j = 0; j < n; j++) {
                    for (unsigned int i = 0; i < n; i++) {
                        const float u = aaJitter ? jitter.next () : 0.5f;
                        const float v = aaJitter ? jitter.next () : 0.5f;
                        const float sx = (static_cast<float> (i) + u) / static_cast<float> (n);
                        const float sy = (static_cast<float> (j) + v) / static_cast<float> (n);
                        color += trace (scene, camera.primaryRay (x, y, width, height, sx, sy), stats, samplers);
                    }
                }
                color /= static_cast<float> (n * n);
            }
            image.setPixel (x, y, toByte (color[0]), toByte (color[1]), toByte (color[2]));
        }
    }
}

Image RayTracer::render (const Scene & scene, const Camera & camera,
                         unsigned int width, unsigned int height) {
    Image image (width, height, Image::RGB888);
    Stats stats;
    const auto start = std::chrono::steady_clock::now ();
    renderRegion (scene, camera, width, height, 0, 0, width, height, image, stats);
    stats.seconds = std::chrono::duration<double> (std::chrono::steady_clock::now () - start).count ();
    lastStats = stats;
    return image;
}
