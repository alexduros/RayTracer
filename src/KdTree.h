// ---------------------------------------------------------
// KdTree Class
// Author : Alexandre Duros (alexandre.duros@gmail.com).
// Copyright (C) 2013 Alexandre Duros.
// All rights reserved.
// ---------------------------------------------------------

#ifndef KDTREE_H
#define KDTREE_H

#define MAX_DEPTH 15

#include <vector>
#include "Mesh.h"
#include "Ray.h"
#include "BoundingBox.h"


using namespace std;

class KdTree {

public :

    inline KdTree() {}
    inline KdTree(const Mesh & m,
                  unsigned int d,
                  int s)
        :mesh(m), maxDepth(d), step(s) {}

    inline virtual ~KdTree(){}
    Mesh & getMesh() { return mesh; }
    BoundingBox & getBoundingBox() { return bbox; }
    bool isLeaf() { return leaf; }
    inline Vertex getMiddle() { return mesh.getVertices()[mesh.getVertices().size()/2]; }
    inline int getDepth(){
        // Note: 'this' pointer cannot be null in well-defined C++
        // This check has been removed as it's not needed in modern C++
        return (1 + rightTree->getDepth() );
    }

    void build();

    bool hasHit(const Ray & r);
    bool searchHit(const Ray & r, Vertex & hit, float & distance);

    void recDrawBoundingBox(unsigned int profondeur);
    void renderGL (unsigned int depth) const;

private :
    Mesh mesh;
    unsigned int maxDepth;
    unsigned int step;
    bool leaf;

    BoundingBox bbox;
    KdTree* leftTree;
    KdTree* rightTree;
};

#endif // KDTREE_H
