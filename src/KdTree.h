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
    bool isLeaf() { return isLeaf; }
    inline Vertex getMiddle() { return vertices[vertices.size()/2]; }
    inline int getDepth(){
        if(this == NULL){
            return 0;
        }
        return (1 + this->Td->getDepth() );
    };

    bool hasHit(const Ray & r);
    bool searchHit(Ray & r, Vertex & intersectionPoint, float & distance);

    void recDrawBoundingBox(unsigned int profondeur);
    void renderGL (unsigned int depth) const;

private :
    Mesh mesh;
    unsigned int maxDepth;
    int step;
    bool isLeaf;

    BoundingBox bbox;
    KdTree leftTree;
    KdTree rightTree;
};

#endif // KDTREE_H
