// *********************************************************
// Ray Tracer Class
// Author : Tamy Boubekeur (boubek@gmail.com).
// Copyright (C) 2010 Tamy Boubekeur.
// All rights reserved.
// *********************************************************

#ifndef RAYTRACER_H
#define RAYTRACER_H

#include "Vec3D.h"
#include "Ray.h"
#include "Scene.h"
#include "Image.h"

class RayTracer {
public:
    // Debug visualization modes. LIT is the real shading path (currently an
    // ambient-only stub); the others colorize the hit information directly so
    // rays, intersections and normals can be checked independently.
    enum class DebugMode {
        LIT,          // ambient-only stub, deliberately dark
        AMBIENT,      // flat material color
        HIT_MASK,     // white = hit, black = miss
        NORMALS,      // (n + 1) / 2 as RGB
        DEPTH,        // hit distance / depthRange, blue (near) -> red (far)
        OBJECT_ID     // distinct color per object index
    };

    /// Per-render counters, refreshed by every render() call.
    struct Stats {
        unsigned long rays = 0;
        unsigned long hits = 0;
        float minHitDist = 0.f;
        float maxHitDist = 0.f;
        double seconds = 0.0;
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
    inline void setDepthRange (float maxDist) { depthRange = maxDist; }
    inline float getDepthRange () const { return depthRange; }
    inline void setBackgroundColor (const Vec3Df & c) { backgroundColor = c; }
    inline const Vec3Df & getBackgroundColor () const { return backgroundColor; }
    inline const Stats & getLastStats () const { return lastStats; }

    /// Brute-force closest front-facing triangle over every object.
    bool closestHit (const Scene & scene, const Ray & ray, Hit & hit) const;

    /// Color of one ray in linear [0,1] RGB (background if nothing is hit).
    Vec3Df trace (const Scene & scene, const Ray & ray);

    Image render (const Scene & scene,
                  const Vec3Df & camPos,
                  const Vec3Df & viewDirection,
                  const Vec3Df & upVector,
                  const Vec3Df & rightVector,
                  float fieldOfView,
                  float aspectRatio,
                  unsigned int screenWidth,
                  unsigned int screenHeight);

private:
    Vec3Df shade (const Scene & scene, const Ray & ray, const Hit & hit) const;

    DebugMode debugMode = DebugMode::AMBIENT;
    float depthRange = 10.f;
    Vec3Df backgroundColor = Vec3Df (0.f, 0.f, 0.f);
    Stats lastStats;
};

#endif // RAYTRACER_H
