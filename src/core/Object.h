// *********************************************************
// Object Class
// Author : Tamy Boubekeur (boubek@gmail.com).
// Copyright (C) 2010 Tamy Boubekeur.
// All rights reserved.
// *********************************************************

#ifndef OBJECT_H
#define OBJECT_H

#include "Mesh.h"
#include "Material.h"
#include "BoundingBox.h"
#include "Bvh.h"

/// A mesh with a material. The bounding box and the BVH are built at
/// construction.
class Object {
public:
    inline Object () {}
    inline Object (const Mesh & mesh, const Material & mat)
        : mesh (mesh), mat (mat) {
        update ();
    }
    virtual ~Object () {}

    inline const Mesh & getMesh () const { return mesh; }
    inline Mesh & getMesh () { return mesh; }

    inline const Material & getMaterial () const { return mat; }
    inline Material & getMaterial () { return mat; }

    inline const BoundingBox & getBoundingBox () const { return bbox; }
    /// Acceleration structure over the mesh's triangles (RayTracer::closestHit).
    inline const Bvh & getBvh () const { return bvh; }
    /// Recompute the bounding box and rebuild the BVH from the mesh. Call it
    /// after editing the mesh in place, as Scene::setUpAxis does: a stale BVH
    /// misses triangles that moved.
    void update ();

    /// Backdrops (the ground plane) are scenery: Scene leaves them out of its
    /// bounding box so framing and the light rig keep following the model.
    inline bool isBackdrop () const { return backdrop; }
    inline void setBackdrop (bool b) { backdrop = b; }

private:
    Mesh mesh;
    Material mat;
    BoundingBox bbox;
    Bvh bvh;
    bool backdrop = false;
};

#endif // OBJECT_H
