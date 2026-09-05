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

} // namespace

bool RayTracer::closestHit (const Scene & scene, const Ray & ray, Hit & best) const {
    bool found = false;
    best.distance = std::numeric_limits<float>::max ();
    const std::vector<Object> & objects = scene.getObjects ();
    for (unsigned int i = 0; i < objects.size (); ++i) {
        Vertex v;
        float distSq = 0.f;   // Ray::nearestHit: 0 = "no hit yet", returns squared distance
        if (ray.nearestHit (objects[i].getMesh (), v, distSq)) {
            const float d = std::sqrt (distSq);
            if (d < best.distance) {
                best.distance = d;
                best.vertex = v;
                best.objectIndex = i;
                found = true;
            }
        }
    }
    return found;
}

Vec3Df RayTracer::shade (const Scene & scene, const Ray & /*ray*/, const Hit & hit) const {
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
            const float t = depthRange > 0.f ? std::min (hit.distance / depthRange, 1.f) : 0.f;
            return Vec3Df (t, 0.f, 1.f - t);
        }
        case DebugMode::OBJECT_ID: {
            // Cheap hash so indices 0..N map to distinct-ish colors.
            const unsigned int i = hit.objectIndex;
            return Vec3Df (std::fmod (i * 157.f, 256.f),
                           std::fmod (i * 97.f + 64.f, 256.f),
                           std::fmod (i * 43.f + 128.f, 256.f)) / 255.f;
        }
        case DebugMode::AMBIENT:
            return mat.getColor ();
        case DebugMode::LIT:
        default:
            // Ambient-only stub; the diffuse/specular terms are not ported yet.
            return 0.33f * mat.getColor ();
    }
}

Vec3Df RayTracer::trace (const Scene & scene, const Ray & ray) {
    lastStats.rays++;
    Hit hit;
    if (!closestHit (scene, ray, hit))
        return backgroundColor;
    lastStats.hits++;
    if (lastStats.hits == 1 || hit.distance < lastStats.minHitDist)
        lastStats.minHitDist = hit.distance;
    if (hit.distance > lastStats.maxHitDist)
        lastStats.maxHitDist = hit.distance;
    return shade (scene, ray, hit);
}

Image RayTracer::render (const Scene & scene,
                         const Vec3Df & camPos,
                         const Vec3Df & direction,
                         const Vec3Df & upVector,
                         const Vec3Df & rightVector,
                         float fieldOfView,
                         float aspectRatio,
                         unsigned int screenWidth,
                         unsigned int screenHeight) {
    Image image (screenWidth, screenHeight, Image::RGB888);
    lastStats = Stats ();
    const auto start = std::chrono::steady_clock::now ();

    const float tanX = std::tan (fieldOfView);
    const float tanY = tanX / aspectRatio;
    for (unsigned int i = 0; i < screenWidth; i++) {
        for (unsigned int j = 0; j < screenHeight; j++) {
            Vec3Df stepX = (float (i) - screenWidth / 2.f) / screenWidth * tanX * rightVector;
            Vec3Df stepY = (float (j) - screenHeight / 2.f) / screenHeight * tanY * upVector;
            Vec3Df dir = direction + stepX + stepY;
            dir.normalize ();
            const Vec3Df color = trace (scene, Ray (camPos, dir));
            image.setPixel (i, (screenHeight - 1) - j, toByte (color[0]), toByte (color[1]), toByte (color[2]));
        }
    }

    lastStats.seconds = std::chrono::duration<double> (std::chrono::steady_clock::now () - start).count ();
    return image;
}
