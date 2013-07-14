// *********************************************************
// Object Class
// Author : Tamy Boubekeur (boubek@gmail.com).
// Copyright (C) 2010 Tamy Boubekeur.
// All rights reserved.
// *********************************************************

#ifndef OBJECT_H
#define OBJECT_H

#include <iostream>
#include <vector>

#include "Mesh.h"
#include "Material.h"
#include "BoundingBox.h"
#include "KdTree.h"


class Object {
public:
    inline Object () {}
    inline Object (const Mesh & mesh, const Material & mat, const KdTree & kdTree) :mesh (mesh), mat (mat), kdTree (kdTree) {
        updateBoundingBox ();
    }
    virtual ~Object () {}

    inline const Mesh & getMesh () const { return mesh; }
    inline Mesh & getMesh () { return mesh; }

    void setMeshAO(int numVertex, float ao){
        mesh.setMeshAO(numVertex,ao);
    }
    inline void updateMesh(const Mesh & newMesh){
        mesh.setVertices(newMesh.getVertices());
    }

    inline const Material & getMaterial () const { return mat; }
    inline Material & getMaterial () { return mat; }

    inline const KdTree & getKdTree () const{ return kdTree; }
    inline KdTree & getKdTree () { return kdTree; }

    inline const BoundingBox & getBoundingBox () const { return bbox; }
    void updateBoundingBox ();
    inline void parcoursObject(Ray r, Vertex & intersectionPoint, float & distance){
        kdTree.searchHit(r,intersectionPoint,distance);
    }
    
private:
    Mesh mesh;
    Material mat;
    BoundingBox bbox;
    KdTree kdTree;
};


#endif // Scene_H

// Some Emacs-Hints -- please don't remove:
//
//  Local Variables:
//  mode:C++
//  tab-width:4
//  End:
