// *********************************************************
// Ray Class
// Author : Tamy Boubekeur (boubek@gmail.com).
// Copyright (C) 2010 Tamy Boubekeur.
// All rights reserved.
// *********************************************************

#ifndef RAY_H
#define RAY_H

#include <iostream>
#include <vector>

#include "Vec3D.h"
#include "BoundingBox.h"
#include "Triangle.h"
#include "Mesh.h"
//#include "Object.h"



class Ray {
public:
    inline Ray () {}
    inline Ray (const Vec3Df & origin, const Vec3Df & direction)
            : origin (origin), direction (direction) {}
    inline virtual ~Ray () {}

    inline const Vec3Df & getOrigin () const { return origin; }
    inline Vec3Df & getOrigin () { return origin; }
    inline const Vec3Df & getDirection () const { return direction; }
    inline Vec3Df & getDirection () { return direction; }


    ////////////////////////////////////////////////////////////////////////////
    //
    //	    Point d'intersection avec la normale
    //
    ////////////////////////////////////////////////////////////////////////////
    bool intersect (const BoundingBox & bbox, Vec3Df & intersectionPoint) const;
    inline bool intersect (const BoundingBox & bbox) const {
        const Vec3Df & v;
        return intersect(bbox, v);
    }
    bool hit (const Mesh & mesh, Vertex & intersectionPoint , float distance);
    bool hit (const Triangle & t, const Mesh & mesh, Vertex & intersectionPoint );

    bool existeIntersection_v (const Mesh & mesh, const std::vector<Triangle> & triangles);
    bool existeIntersectionVecteur (const Mesh & mesh, const std::vector<Triangle> & triangles);
private:
    Vec3Df origin;
    Vec3Df direction;
};


#endif // RAY_H

// Some Emacs-Hints -- please don't remove:
//
//  Local Variables:
//  mode:C++
//  tab-width:4
//  End:
