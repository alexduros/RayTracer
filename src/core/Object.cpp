// *********************************************************
// Object Class
// Author : Tamy Boubekeur (boubek@gmail.com).
// Copyright (C) 2010 Tamy Boubekeur.
// All rights reserved.
// *********************************************************

#include "Object.h"

void Object::update () {
    if (primitive) {
        bbox = primitive->boundingBox ();
        bvh = Bvh ();  // a primitive needs no tree: one equation, one root
        return;
    }
    bbox = BoundingBox::computeBoundingBox (mesh.getVertices ());
    bvh.build (mesh);
}
