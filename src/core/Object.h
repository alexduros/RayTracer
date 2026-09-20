// *********************************************************
// Object Class
// Author : Tamy Boubekeur (boubek@gmail.com).
// Copyright (C) 2010 Tamy Boubekeur.
// All rights reserved.
// *********************************************************

#ifndef OBJECT_H
#define OBJECT_H

#include <memory>

#include "Mesh.h"
#include "Material.h"
#include "BoundingBox.h"
#include "Bvh.h"
#include "Primitive.h"

/// A mesh with a material, or an analytic primitive with one (Primitive.h):
/// a sphere is an equation, not a tessellation. The bounding box and, for a
/// mesh, the BVH are built at construction.
class Object {
public:
    inline Object () {}
    inline Object (const Mesh & mesh, const Material & mat)
        : mesh (mesh), mat (mat) {
        update ();
    }
    /// An analytic surface instead of triangles. Shared and never modified,
    /// so copying the Object (as RenderJob does) stays cheap.
    inline Object (const std::shared_ptr<const Primitive> & primitive, const Material & mat)
        : primitive (primitive), mat (mat) {
        update ();
    }
    virtual ~Object () {}

    inline const Mesh & getMesh () const { return mesh; }
    inline Mesh & getMesh () { return mesh; }

    inline const Material & getMaterial () const { return mat; }
    inline Material & getMaterial () { return mat; }

    inline const BoundingBox & getBoundingBox () const { return bbox; }
    /// Acceleration structure over the mesh's triangles (RayTracer::closestHit).
    /// Empty for a primitive, which is its own intersection test.
    inline const Bvh & getBvh () const { return bvh; }
    /// The analytic surface this object is, or nullptr when it is a mesh.
    inline const Primitive * getPrimitive () const { return primitive.get (); }
    /// Recompute the bounding box and rebuild the BVH from the mesh. Call it
    /// after editing the mesh in place, as Scene::setUpAxis does: a stale BVH
    /// misses triangles that moved.
    void update ();

    /// Backdrops (the ground plane) are scenery: Scene leaves them out of its
    /// bounding box so framing and the light rig keep following the model.
    inline bool isBackdrop () const { return backdrop; }
    inline void setBackdrop (bool b) { backdrop = b; }

private:
    Mesh mesh;                                   // empty for a primitive
    std::shared_ptr<const Primitive> primitive;  // null for a mesh
    Material mat;
    BoundingBox bbox;
    Bvh bvh;
    bool backdrop = false;
};

#endif // OBJECT_H
