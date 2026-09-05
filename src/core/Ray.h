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

    /// Ray / triangle test with back-face culling. On success `hit` holds the
    /// interpolated position, normal and AO coefficient.
    bool hit (const Triangle & triangle, const Mesh & mesh, Vertex & hit) const;

    /// Closest front-facing triangle of `mesh`. `distance` is the *squared*
    /// distance from the origin and must be 0 on entry (0 means "no hit yet").
    bool nearestHit (const Mesh & mesh, Vertex & hit, float & distance) const;

private:
    Vec3Df origin;
    Vec3Df direction;
};

#endif // RAY_H
