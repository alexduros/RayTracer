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

/// A mesh with a material. The bounding box is cached at construction.
class Object {
public:
    inline Object () {}
    inline Object (const Mesh & mesh, const Material & mat)
        : mesh (mesh), mat (mat) {
        updateBoundingBox ();
    }
    virtual ~Object () {}

    inline const Mesh & getMesh () const { return mesh; }
    inline Mesh & getMesh () { return mesh; }

    inline const Material & getMaterial () const { return mat; }
    inline Material & getMaterial () { return mat; }

    inline const BoundingBox & getBoundingBox () const { return bbox; }
    void updateBoundingBox ();

    /// Backdrops (the ground plane) are scenery: Scene leaves them out of its
    /// bounding box so framing and the light rig keep following the model.
    inline bool isBackdrop () const { return backdrop; }
    inline void setBackdrop (bool b) { backdrop = b; }

private:
    Mesh mesh;
    Material mat;
    BoundingBox bbox;
    bool backdrop = false;
};

#endif // OBJECT_H
