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
#include <random>

namespace {

inline unsigned char toByte (float c) {
    int v = static_cast<int> (c * 255.f + 0.5f);
    return static_cast<unsigned char> (std::max (0, std::min (255, v)));
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

/// Per-pixel seed for jittered sampling: depends on the pixel only, so the
/// result does not change with tile order or thread count.
unsigned int pixelSeed (unsigned int x, unsigned int y) {
    return (x * 73856093u) ^ (y * 19349663u) ^ 0x9E3779B9u;
}

/// Uniform in [0, 1) from the raw engine output. std::minstd_rand is fully
/// specified by the standard, unlike the distributions, so this is the same
/// on every platform.
float uniform01 (std::minstd_rand & rng) {
    return static_cast<float> (rng () - std::minstd_rand::min ()) /
           (static_cast<float> (std::minstd_rand::max () - std::minstd_rand::min ()) + 1.f);
}

const RayTracer::ModeInfo kModeInfos[RayTracer::kModeCount] = {
    {"lit", "Lit (Lambert + Blinn-Phong, shadows)",
     "For each light: material colour x light colour x max(0, n.l), the cosine between the normal and the light "
     "direction (Lambert); plus a white highlight where the half-vector between the light and view directions "
     "lines up with the normal, raised to the shininess (Blinn-Phong); a shadow ray toward the light drops it "
     "when something is in the way; plus a constant ambient term. No bounces yet.",
     "Brighter where a surface faces a light; tight bright spots are highlights; where a light is blocked only "
     "the ambient term and the other lights remain. Colour is material x light, so the cyan key light tints the "
     "orange default material green. Turn on the ground plane to see the shadows fall.",
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

} // namespace

const RayTracer::ModeInfo & RayTracer::info (DebugMode mode) {
    const int i = static_cast<int> (mode);
    return kModeInfos[(i >= 0 && i < kModeCount) ? i : 0];
}

const RayTracer::ModeInfo & RayTracer::antiAliasingInfo () {
    return kAntiAliasingInfo;
}

namespace {

// The roadmap, one sentence of principle each. Order = suggested order of
// implementation within each family; the document groups them.
const RayTracer::ModeInfo kPlannedModes[RayTracer::kPlannedModeCount] = {
    {"softshadows", "Soft shadows (area lights)",
     "Many shadow rays toward points spread over the light's disk estimate the fraction of it that is visible, "
     "giving penumbrae instead of hard edges.",
     "Needs hard shadows and the per-pixel sampling from anti-aliasing. Experiment 6.",
     "R. L. Cook, T. Porter & L. Carpenter, \"Distributed Ray Tracing\", SIGGRAPH 1984."},
    {"ao", "Ambient occlusion",
     "Rays cast over the hemisphere around the normal measure how open the surroundings are, darkening creases "
     "and contact points.",
     "Needs hemisphere sampling and a BVH to stay fast. Experiment 8.",
     "S. Zhukov, A. Iones & G. Kronin, \"An Ambient Light Illumination Model\", Eurographics Rendering Workshop "
     "1998."},
    {"reflection", "Mirror reflections",
     "Rays bounce off reflective surfaces recursively and add what they see, scaled by the material's "
     "reflectivity, up to a depth limit.",
     "Needs a reflectivity value on Material and recursion in shade(). Experiment 7.",
     "T. Whitted, \"An Improved Illumination Model for Shaded Display\", CACM 23(6), 1980."},
    {"refraction", "Refraction (glass)",
     "Rays bend through transparent surfaces following Snell's law and split between reflection and "
     "transmission by the Fresnel term.",
     "Needs reflections, an index of refraction (MTL Ni) and transparency (MTL d). Experiment 7.",
     "T. Whitted, CACM 23(6), 1980; C. Schlick, \"An Inexpensive BRDF Model for Physically-based Rendering\", "
     "Computer Graphics Forum 13(3), 1994."},
    {"pathtracing", "Path tracing (global illumination)",
     "Each pixel averages many random light paths bouncing through the scene, converging on the rendering "
     "equation with indirect light and colour bleeding.",
     "Needs emissive materials, a BVH, many samples per pixel and the progressive display that already exists.",
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
     "Needs counters in closestHit and in the BVH; the diagnostic that justifies experiment 3.",
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
    const std::vector<Object> & objects = scene.getObjects ();
    for (unsigned int i = 0; i < objects.size (); ++i) {
        Vertex v;
        float t = 0.f;
        if (ray.nearestHit (objects[i].getMesh (), v, t) && t < best.distance) {
            best.distance = t;
            best.vertex = v;
            best.objectIndex = i;
            found = true;
        }
    }
    return found;
}

Vec3Df RayTracer::shade (const Scene & scene, const Ray & ray, const Hit & hit) const {
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
        case DebugMode::LIT:
        default: {
            // Lambert diffuse + Blinn-Phong highlight + hard shadows. No
            // attenuation, no bounces.
            Vec3Df n = hit.vertex.getNormal ();
            n.normalize ();
            const Vec3Df & p = hit.vertex.getPos ();
            Vec3Df v = -ray.getDirection ();
            v.normalize ();
            // Shadow rays start a little off the surface along the normal so
            // the surface cannot shadow itself ("acne"); scaled to the model.
            const float bias = 1e-4f * std::max (scene.getBoundingBox ().getSize (), 1e-3f);
            const Vec3Df shadowOrigin = p + n * bias;

            Vec3Df color = ambientIntensity * mat.getColor ();
            for (const Light & light : scene.getLights ()) {
                Vec3Df l = light.getPos () - p;
                const float distanceToLight = l.normalize ();
                const float nDotL = Vec3Df::dotProduct (n, l);
                if (nDotL <= 0.f)
                    continue;  // light behind the surface
                if (shadows) {
                    Hit blocker;
                    if (closestHit (scene, Ray (shadowOrigin, l), blocker) && blocker.distance < distanceToLight)
                        continue;  // something between the point and the light
                }
                color += (mat.getDiffuse () * light.getIntensity () * nDotL) * (mat.getColor () * light.getColor ());
                if (specularEnabled && mat.getSpecular () > 0.f) {
                    Vec3Df h = l + v;
                    h.normalize ();
                    const float nDotH = std::max (0.f, Vec3Df::dotProduct (n, h));
                    color += (mat.getSpecular () * light.getIntensity () * std::pow (nDotH, mat.getShininess ())) *
                             light.getColor ();
                }
            }
            return color;
        }
    }
}

Vec3Df RayTracer::trace (const Scene & scene, const Ray & ray, Stats & stats) const {
    stats.rays++;
    Hit hit;
    if (!closestHit (scene, ray, hit))
        return backgroundColor;
    stats.hits++;
    if (stats.hits == 1 || hit.distance < stats.minHitDist)
        stats.minHitDist = hit.distance;
    if (hit.distance > stats.maxHitDist)
        stats.maxHitDist = hit.distance;
    return shade (scene, ray, hit);
}

void RayTracer::renderRegion (const Scene & scene, const Camera & camera,
                              unsigned int width, unsigned int height,
                              unsigned int x0, unsigned int y0, unsigned int x1, unsigned int y1,
                              Image & image, Stats & stats) const {
    const unsigned int n = std::max (1u, aaSamplesPerAxis);
    for (unsigned int y = y0; y < y1; y++) {
        for (unsigned int x = x0; x < x1; x++) {
            Vec3Df color;
            if (n == 1) {
                // One ray through the pixel centre.
                color = trace (scene, camera.primaryRay (x, y, width, height), stats);
            } else {
                // n x n sub-pixel grid, optionally jittered inside each cell.
                std::minstd_rand rng (pixelSeed (x, y));
                for (unsigned int j = 0; j < n; j++) {
                    for (unsigned int i = 0; i < n; i++) {
                        const float u = aaJitter ? uniform01 (rng) : 0.5f;
                        const float v = aaJitter ? uniform01 (rng) : 0.5f;
                        const float sx = (static_cast<float> (i) + u) / static_cast<float> (n);
                        const float sy = (static_cast<float> (j) + v) / static_cast<float> (n);
                        color += trace (scene, camera.primaryRay (x, y, width, height, sx, sy), stats);
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
