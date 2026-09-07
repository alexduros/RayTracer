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
#include "Image.h"

class RayTracer {
public:
    // LIT is the shading path; the others colorize the hit information
    // directly so rays, intersections and normals can be checked one at a time.
    enum class DebugMode {
        LIT,          // Lambert: ambient + sum over lights of diffuse * max(0, n.l)
        AMBIENT,      // flat material color
        HIT_MASK,     // white = hit, black = miss
        NORMALS,      // (n + 1) / 2 as RGB
        DEPTH,        // (distance - depthNear) / (depthFar - depthNear): blue (near) -> red (far)
        OBJECT_ID     // distinct color per object index
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
        float distance = 0.f;   // Euclidean distance from the ray origin
        Vertex vertex;          // interpolated position / normal
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
    static constexpr int kModeCount = 6;
    static const ModeInfo & info (DebugMode mode);
    /// Same account for anti-aliasing, which is a setting rather than a mode.
    static const ModeInfo & antiAliasingInfo ();

    /// Modes the raytracer could offer next (claudedocs/RENDERING_ROADMAP.md
    /// is the full map): same fields, with `reading` holding what the mode
    /// needs. Listed greyed out in the viewer's mode menu and in --help so
    /// the roadmap is visible where the modes are chosen.
    static constexpr int kPlannedModeCount = 12;
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

    /// Hard shadows: a shadow ray toward each light drops that light when
    /// something is in the way (Lit mode only).
    inline void setShadows (bool on) { shadows = on; }
    inline bool getShadows () const { return shadows; }
    /// Blinn-Phong highlight from Material::specular / shininess (Lit mode only).
    inline void setSpecularEnabled (bool on) { specularEnabled = on; }
    inline bool isSpecularEnabled () const { return specularEnabled; }

    inline void setDebugMode (DebugMode m) { debugMode = m; }
    inline DebugMode getDebugMode () const { return debugMode; }
    inline void setDepthRange (float near, float far) { depthNear = near; depthFar = far; }
    inline float getDepthNear () const { return depthNear; }
    inline float getDepthFar () const { return depthFar; }
    inline void setAmbientIntensity (float a) { ambientIntensity = a; }
    inline float getAmbientIntensity () const { return ambientIntensity; }
    inline void setBackgroundColor (const Vec3Df & c) { backgroundColor = c; }
    inline const Vec3Df & getBackgroundColor () const { return backgroundColor; }
    inline const Stats & getLastStats () const { return lastStats; }

    /// Brute-force closest front-facing triangle over every object.
    bool closestHit (const Scene & scene, const Ray & ray, Hit & hit) const;

    /// Color of one ray in linear [0,1] RGB (background if nothing is hit),
    /// counting it in `stats`. Const and reentrant: safe from several threads.
    Vec3Df trace (const Scene & scene, const Ray & ray, Stats & stats) const;

    /// Color for a known hit, in linear [0,1] RGB, according to the mode.
    Vec3Df shade (const Scene & scene, const Ray & ray, const Hit & hit) const;

    /// Trace pixels [x0, x1) x [y0, y1) of a width x height frame into `image`
    /// (which must already have that size), accumulating `stats`. Pixels are
    /// independent, so any partition of the frame gives the same picture; this
    /// is what RenderJob calls per tile.
    void renderRegion (const Scene & scene, const Camera & camera,
                       unsigned int width, unsigned int height,
                       unsigned int x0, unsigned int y0, unsigned int x1, unsigned int y1,
                       Image & image, Stats & stats) const;

    /// Synchronous full-frame render; the reference the tests compare against.
    /// Records the statistics in getLastStats().
    Image render (const Scene & scene, const Camera & camera,
                  unsigned int width, unsigned int height);

private:
    DebugMode debugMode = DebugMode::LIT;
    float depthNear = 0.f;
    float depthFar = 10.f;
    float ambientIntensity = 0.15f;
    Vec3Df backgroundColor = Vec3Df (0.f, 0.f, 0.f);
    unsigned int aaSamplesPerAxis = 1;
    bool aaJitter = false;
    bool shadows = true;
    bool specularEnabled = true;
    Stats lastStats;
};

#endif // RAYTRACER_H
