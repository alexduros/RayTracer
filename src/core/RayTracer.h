// *********************************************************
// Ray Tracer Class
// Author : Tamy Boubekeur (boubek@gmail.com).
// Copyright (C) 2010 Tamy Boubekeur.
// All rights reserved.
// *********************************************************

#ifndef RAYTRACER_H
#define RAYTRACER_H

#include <algorithm>

#include "Vec3D.h"
#include "Camera.h"
#include "Ray.h"
#include "Scene.h"
#include "Display.h"
#include "HdrImage.h"
#include "Image.h"
#include "Sampler.h"

class RayTracer {
public:
    // LIT is the shading path; the others colorize the hit information
    // directly so rays, intersections and normals can be checked one at a time.
    enum class DebugMode {
        LIT,          // ambient + per light Lambert + Blinn-Phong (shadowed, occluded), then mirror and glass rays
        AMBIENT,      // flat material color
        HIT_MASK,     // white = hit, black = miss
        NORMALS,      // (n + 1) / 2 as RGB
        DEPTH,        // (distance - depthNear) / (depthFar - depthNear): blue (near) -> red (far)
        OBJECT_ID,    // distinct color per object index
        AMBIENT_OCCLUSION  // open share of the hemisphere as grey (white = open)
    };

    /// Per-render counters, refreshed by every render() call.
    struct Stats {
        unsigned long rays = 0;
        unsigned long hits = 0;
        float minHitDist = 0.f;
        float maxHitDist = 0.f;
        double seconds = 0.0;

        /// Fold in the counters of another region (tile, thread). `seconds`
        /// is left alone: it is wall time, not additive.
        inline void accumulate (const Stats & other) {
            if (other.hits) {
                if (hits == 0 || other.minHitDist < minHitDist)
                    minHitDist = other.minHitDist;
                if (other.maxHitDist > maxHitDist)
                    maxHitDist = other.maxHitDist;
            }
            rays += other.rays;
            hits += other.hits;
        }
    };

    /// Closest intersection of a ray with the scene.
    struct Hit {
        unsigned int objectIndex = 0;
        unsigned int triangleIndex = 0;  // in the object's mesh
        float distance = 0.f;   // Euclidean distance from the ray origin
        Vertex vertex;          // interpolated position / normal
        bool backFace = false;  // met from behind, by a two-sided ray: leaving the object
    };

    /// What a mode computes, how to read the picture, and the study it comes
    /// from. Shared by the GUI, the CLI and the docs so every mode explains
    /// itself instead of looking arbitrary.
    struct ModeInfo {
        const char * slug;       // command-line name
        const char * name;       // label shown in the UI
        const char * principle;  // what is computed
        const char * reading;    // how to interpret the colours
        const char * reference;  // the paper, thesis or book chapter
    };
    static constexpr int kModeCount = 7;
    static const ModeInfo & info (DebugMode mode);
    /// Same account for anti-aliasing, which is a setting rather than a mode.
    static const ModeInfo & antiAliasingInfo ();
    /// ... and for soft shadows.
    static const ModeInfo & softShadowsInfo ();
    /// ... and for mirror reflections.
    static const ModeInfo & reflectionsInfo ();
    /// ... and for refraction (glass).
    static const ModeInfo & refractionInfo ();
    /// ... and for the display: exposure, tone curve, encoding.
    static const ModeInfo & toneMappingInfo ();

    /// Modes the raytracer could offer next (claudedocs/RENDERING_ROADMAP.md
    /// is the full map): same fields, with `reading` holding what the mode
    /// needs. Listed greyed out in the viewer's mode menu and in --help so
    /// the roadmap is visible where the modes are chosen.
    static constexpr int kPlannedModeCount = 7;
    static const ModeInfo & plannedMode (int index);

    RayTracer () {}

    /// Anti-aliasing: samplesPerAxis x samplesPerAxis primary rays per pixel
    /// on a regular sub-pixel grid (1 = one ray through the pixel centre),
    /// averaged in linear colour. With `jitter` each ray is offset randomly
    /// inside its grid cell using a seed derived from the pixel, so the
    /// picture is reproducible and independent of tile order or threads.
    inline void setAntiAliasing (unsigned int samplesPerAxis, bool jitter) {
        aaSamplesPerAxis = std::max (1u, samplesPerAxis);
        aaJitter = jitter;
    }
    inline unsigned int getAntiAliasingSamplesPerAxis () const { return aaSamplesPerAxis; }
    inline bool getAntiAliasingJitter () const { return aaJitter; }

    /// Shadows: shadow rays toward each light scale it by the fraction that
    /// gets through (Lit mode only). Hard unless setShadowSamples asks for more.
    inline void setShadows (bool on) { shadows = on; }
    inline bool getShadows () const { return shadows; }
    /// Soft shadows: a light with a radius becomes a disk facing the shaded
    /// point, sampled by samplesPerAxis x samplesPerAxis shadow rays, one
    /// jittered ray per cell of a grid over the disk. 1 = a single ray toward
    /// the light's centre, i.e. hard shadows (the default), which is also what
    /// a light of radius 0 gets. Samples are seeded per pixel, so renders stay
    /// reproducible and independent of tile order.
    inline void setShadowSamples (unsigned int samplesPerAxis) { shadowSamplesPerAxis = std::max (1u, samplesPerAxis); }
    inline unsigned int getShadowSamplesPerAxis () const { return shadowSamplesPerAxis; }
    /// Ambient occlusion (Lit and AO modes): samplesPerAxis x samplesPerAxis
    /// rays over the hemisphere around the normal, cosine-weighted (one
    /// jittered ray per cell of a grid on the disk, lifted onto the
    /// hemisphere), each blocked if it meets a surface within `radius` (world
    /// units). The open share scales the ambient and diffuse terms, not the
    /// highlight; the AO mode shows it as grey. 0 samples = off (the default),
    /// bit for bit the picture without it (and a white AO mode).
    inline void setAmbientOcclusion (unsigned int samplesPerAxis, float radius) {
        aoSamplesPerAxis = samplesPerAxis;
        aoRadius = std::max (0.f, radius);
    }
    inline unsigned int getAmbientOcclusionSamplesPerAxis () const { return aoSamplesPerAxis; }
    inline float getAmbientOcclusionRadius () const { return aoRadius; }
    /// Mirror reflections and glass (Lit mode only). On a material with a
    /// reflectivity k > 0 the colour becomes (1 - k) x its own shading + k x
    /// what the mirrored ray sees. On a transparency g > 0, (1 - g) x its own
    /// shading + g x clear glass: the Fresnel share F of the mirrored ray +
    /// (1 - F) x the refracted one (Snell's law, Material::getIor; two-sided
    /// inside the object, so it can leave). Rays are followed through at most
    /// maxDepth bounces; at the limit a surface shows its own shading, so 0
    /// turns both off, bit for bit. Default 8: glass takes several bounces
    /// to get through a model and out.
    inline void setMaxDepth (unsigned int depth) { maxDepth = depth; }
    inline unsigned int getMaxDepth () const { return maxDepth; }
    /// Blinn-Phong highlight from Material::specular / shininess (Lit mode only).
    inline void setSpecularEnabled (bool on) { specularEnabled = on; }
    inline bool isSpecularEnabled () const { return specularEnabled; }
    /// Traverse each object's BVH instead of testing every triangle. Same
    /// hits, same pixels; turn it off only to compare against the brute-force
    /// reference.
    inline void setBvhEnabled (bool on) { bvhEnabled = on; }
    inline bool isBvhEnabled () const { return bvhEnabled; }

    inline void setDebugMode (DebugMode m) { debugMode = m; }
    inline DebugMode getDebugMode () const { return debugMode; }
    inline void setDepthRange (float near, float far) { depthNear = near; depthFar = far; }
    inline float getDepthNear () const { return depthNear; }
    inline float getDepthFar () const { return depthFar; }
    inline void setAmbientIntensity (float a) { ambientIntensity = a; }
    inline float getAmbientIntensity () const { return ambientIntensity; }
    /// How render () turns radiance into bytes: exposure, tone curve,
    /// encoding. The default is linear, clipped at 1, as before displays
    /// existed; the CLI and the viewer default to sRGB.
    inline void setDisplay (const Display & d) { display = d; }
    inline const Display & getDisplay () const { return display; }
    inline void setBackgroundColor (const Vec3Df & c) { backgroundColor = c; }
    inline const Vec3Df & getBackgroundColor () const { return backgroundColor; }
    inline const Stats & getLastStats () const { return lastStats; }

    /// Closest front-facing triangle over every object (either side for a
    /// two-sided ray, then `backFace` tells which), through each object's BVH
    /// or, when it is disabled, by testing every triangle. At equal distance
    /// the lowest object index wins, then the lowest triangle index, so both
    /// paths return the same hit.
    bool closestHit (const Scene & scene, const Ray & ray, Hit & hit) const;

    /// True when a front-facing triangle lies along `ray` strictly closer than
    /// `maxDistance`: the question a shadow ray asks. Stops at the first one
    /// found, and answers exactly as `closestHit (...) && hit.distance <
    /// maxDistance` would, through the BVH or not.
    bool occluded (const Scene & scene, const Ray & ray, float maxDistance) const;

    /// Color of one ray in linear [0,1] RGB (background if nothing is hit),
    /// counting it in `stats`; `samplers` feed the soft shadows and the
    /// occlusion. Const and reentrant: safe from several threads, each with
    /// its own samplers.
    Vec3Df trace (const Scene & scene, const Ray & ray, Stats & stats, PixelSamplers & samplers) const;
    /// Same with fixed-seed samplers: reproducible, for probing single rays.
    Vec3Df trace (const Scene & scene, const Ray & ray, Stats & stats) const;

    /// Color for a known hit, in linear [0,1] RGB, according to the mode.
    /// `depth` counts the reflections that led to this hit (0 for a primary
    /// ray); a reflective surface follows its mirror ray while depth < maxDepth.
    Vec3Df shade (const Scene & scene, const Ray & ray, const Hit & hit, PixelSamplers & samplers,
                  unsigned int depth = 0) const;

    /// Fraction of `light` seen from the surface point `p` with unit normal
    /// `n`, in [0, 1]: one shadow ray toward its centre (0 or 1), or
    /// getShadowSamplesPerAxis ()^2 rays over its disk. The part of the disk
    /// below the surface's horizon counts as hidden.
    float lightVisibility (const Scene & scene, const Vec3Df & p, const Vec3Df & n, const Light & light,
                           Sampler & sampler) const;

    /// Open share of the hemisphere above the surface point `p` with unit
    /// normal `n`, in [0, 1], cosine-weighted: the fraction of
    /// getAmbientOcclusionSamplesPerAxis ()^2 rays that meet nothing within
    /// the radius. 1 when occlusion is off.
    float ambientOcclusion (const Scene & scene, const Vec3Df & p, const Vec3Df & n, Sampler & sampler) const;

    /// Trace pixels [x0, x1) x [y0, y1) of a width x height frame into `image`
    /// (which must already have that size) as linear radiance, accumulating
    /// `stats`. Pixels are independent, so any partition of the frame gives
    /// the same picture; this is what RenderJob calls per tile.
    void renderRegion (const Scene & scene, const Camera & camera,
                       unsigned int width, unsigned int height,
                       unsigned int x0, unsigned int y0, unsigned int x1, unsigned int y1,
                       HdrImage & image, Stats & stats) const;

    /// Synchronous full-frame render in linear radiance, above 1 included;
    /// the reference the tests compare against. Records the statistics in
    /// getLastStats().
    HdrImage renderHdr (const Scene & scene, const Camera & camera,
                        unsigned int width, unsigned int height);
    /// renderHdr () through the display, in bytes.
    Image render (const Scene & scene, const Camera & camera,
                  unsigned int width, unsigned int height);

private:
    /// Lit mode's own shading of a surface point: ambient, and per light
    /// Lambert and Blinn-Phong, scaled by shadows and occlusion.
    Vec3Df directLight (const Scene & scene, const Material & mat, const Vec3Df & p, const Vec3Df & n,
                        const Ray & ray, PixelSamplers & samplers) const;
    /// Colour a secondary ray brings back from a hit at `depth`: what it
    /// meets, shaded one bounce deeper, or the background.
    Vec3Df bounce (const Scene & scene, const Ray & ray, PixelSamplers & samplers, unsigned int depth) const;

    DebugMode debugMode = DebugMode::LIT;
    float depthNear = 0.f;
    float depthFar = 10.f;
    // 0.05, not the 0.15 of the linear-bytes years: the display (Display.h)
    // lifts the dark tones now, and 0.15 washed the shadows out.
    float ambientIntensity = 0.05f;
    Vec3Df backgroundColor = Vec3Df (0.f, 0.f, 0.f);
    Display display;
    unsigned int aaSamplesPerAxis = 1;
    bool aaJitter = false;
    bool shadows = true;
    unsigned int shadowSamplesPerAxis = 1;
    unsigned int maxDepth = 8;
    unsigned int aoSamplesPerAxis = 0;
    float aoRadius = 1.f;
    bool specularEnabled = true;
    bool bvhEnabled = true;
    Stats lastStats;
};

#endif // RAYTRACER_H
