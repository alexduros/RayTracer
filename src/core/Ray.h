// *********************************************************
// Ray Class
// Author : Tamy Boubekeur (boubek@gmail.com).
// Copyright (C) 2010 Tamy Boubekeur.
// All rights reserved.
// *********************************************************

#ifndef RAY_H
#define RAY_H

#include "Vec3D.h"
#include "BoundingBox.h"
#include "Triangle.h"
#include "Mesh.h"

class Ray {
public:
    inline Ray () {}
    inline Ray (const Vec3Df & origin, const Vec3Df & direction)
        : origin (origin), direction (direction) {}
    virtual ~Ray () {}

    inline const Vec3Df & getOrigin () const { return origin; }
    inline Vec3Df & getOrigin () { return origin; }
    inline const Vec3Df & getDirection () const { return direction; }
    inline Vec3Df & getDirection () { return direction; }

    /// Slab test against an axis-aligned box; `intersect` receives the entry point.
    bool intersect (const BoundingBox & bbox, Vec3Df & intersect) const;
    inline bool intersect (const BoundingBox & bbox) const {
        Vec3Df v;
        return intersect (bbox, v);
    }

    /// Slab test for BVH traversal, cheap enough to run at every node.
    /// `invDirection` is 1 / direction per axis, computed once per ray (+-inf
    /// on an axis-parallel ray). True when the ray is inside the box somewhere
    /// in [0, tMax]; `tEntry` then receives where it enters (0 if the origin is
    /// inside). An entry exactly at tMax counts, so equal-distance ties are
    /// never pruned.
    inline bool intersect (const BoundingBox & bbox, const Vec3Df & invDirection, float tMax, float & tEntry) const {
        float tEnter = 0.f, tExit = tMax;
        for (int i = 0; i < 3; ++i) {
            // Planes in the order the ray meets them. Each bound is folded in
            // by a comparison a NaN fails, so an origin lying on a plane of
            // an axis the ray is parallel to (0 x inf) keeps the ray inside
            // that slab instead of emptying the interval.
            const bool backward = invDirection[i] < 0.f;
            const float tNear = ((backward ? bbox.getMax ()[i] : bbox.getMin ()[i]) - origin[i]) * invDirection[i];
            const float tFar = ((backward ? bbox.getMin ()[i] : bbox.getMax ()[i]) - origin[i]) * invDirection[i];
            if (tNear > tEnter)
                tEnter = tNear;
            if (tFar < tExit)
                tExit = tFar;
        }
        if (tEnter > tExit)
            return false;
        tEntry = tEnter;
        return true;
    }

    /// Ray / triangle test with back-face culling. On success `hit` holds the
    /// interpolated position, normal and AO coefficient, and `t` the distance
    /// along the ray (Euclidean when `direction` is normalized).
    bool hit (const Triangle & triangle, const Mesh & mesh, Vertex & hit, float & t) const;

    /// Closest front-facing triangle of `mesh`. `hit` and `t` are left
    /// untouched when nothing is hit.
    bool nearestHit (const Mesh & mesh, Vertex & hit, float & t) const;

private:
    Vec3Df origin;
    Vec3Df direction;
};

#endif // RAY_H
