// *********************************************************
// Object Class
// Author : Tamy Boubekeur (boubek@gmail.com).
// Copyright (C) 2010 Tamy Boubekeur.
// All rights reserved.
// *********************************************************

#include "Object.h"

void Object::updateBoundingBox () {
    bbox = BoundingBox::computeBoundingBox (mesh.getVertices ());
}
