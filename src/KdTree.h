// ---------------------------------------------------------
// KdTree Class
// Author : Alexandre Duros (alexandre.duros@gmail.com).
// Copyright (C) 2013 Alexandre Duros.
// All rights reserved.
// ---------------------------------------------------------

#ifndef KDTREE_H
#define KDTREE_H

#define MAX_DEPTH = 15;

#include <vector>
#include "Ray.h"
#include "Object.h"
#include "BoundingBox.h"


using namespace std;

class KdTree {

public :

    inline KdTree() {}
    inline KdTree(const Mesh & m,
                  unsigned int d,
                  int s)
        :mesh(m), maxDepth(d), step(s) {}

    inline virutal ~KdTree(){}
    Mesh & getMesh() { return mesh; }
    BoundingBox & getBoundingBox() { return bbox; }
    inline Vertex getMiddle() { return vertices[vertices.size()/2]; }
    inline int getDepth(){
        if(this == NULL){
            return 0;
        }
        return (1 + this->Td->getDepth() );
    };

    bool recParcoursArbreExistence_v(Ray & r, const Mesh & mesh);
    bool recParcoursArbre_v(Ray & r, const Mesh & mesh, Vertex & intersectionPoint, float & distance);

    void recDrawBoundingBox(unsigned int profondeur);
    void renderGL (unsigned int depth) const;

private :
    Mesh mesh;
    unsigned int maxDepth;
    int step;

    BoundingBox bbox;
    KdTree leftTree;
    KdTree rightTree;
};

#endif // KDTREE_H
