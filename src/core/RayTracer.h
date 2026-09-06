// *********************************************************
// Ray Tracer Class
// Author : Tamy Boubekeur (boubek@gmail.com).
// Copyright (C) 2010 Tamy Boubekeur.
// All rights reserved.
// *********************************************************

#ifndef RAYTRACER_H
#define RAYTRACER_H

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

    RayTracer () {}

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
    Stats lastStats;
};

#endif // RAYTRACER_H
