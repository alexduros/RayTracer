// Copyright (C) 2013 Alexandre Duros.
// All rights reserved.
// ---------------------------------------------------------


#include "KdTree.h"
#include "Vertex.h"
#include "Triangle.h"

using namespace std;

void build () {
    const vector<Triangle> & triangles = mesh.getTrianges();
    const vector<Vertex> & vertices = mesh.getVertices();
    bbox = BoundingBox::computeBoundingBoxTriangles(triangles, mesh);

    if(step > triangles.size() &&  maxDepth <= MAX_DEPTH){
        leaf = false;

        int direction = bbox.getDirection();
        const Vertex & median = Vertex::getMedian(vertices, direction);

        Vertex::sortByDirection(direction);
        std::vector<Vertex> leftVertices;
        std::vector<Vertex> rightVertices;
        Vertex::split(vertices, leftVertices, rightVertices);

        std::vector<Triangle> leftTriangles;
        vector<Triangle> rightTriangles;
        Triangle::split(median, triangles, mesh, axe,
                        leftTriangles, rightTriangles);

        if(leftVertices.size() != 0 && leftTriangles.size() != 0){
            Mesh leftMesh = Mesh(leftTriangles, leftVertices);
            leftTree = KdTree(&leftMesh, maxDepth + 1, step);
        }
        if(rightVertices.size() !=0 && rightTriangles.size() != 0){
            Mesh rightMesh = Mesh(rightTriangles, rightVertices);
            rightTree = new KdTree(&rightMesh, maxDepth + 1, step);
        }
    } else {
        leaf = true;
    }

}

void KdTree::recDrawBoundingBox(unsigned int depth){
    if(maxDepth == depth){
        bbox.renderGL();
    } else {
        TdrecDrawBoundingBox(depth);
        this->Tg->recDrawBoundingBox(depth);
    }
}

void KdTree::renderGL (unsigned int depth) const {
    if (maxDepth <= depth) {
        bbox.renderGL();
    } else if(Tg != NULL && Td != NULL) {
        Tg.renderGL(depth);
        Td->renderGL(depth);
    } else if(Tg == NULL && Td == NULL){
        bbox.renderGL();
    }
}

bool KdTree::hasHit(const Ray & ray){

    if(this==NULL){
        return false;
    } else if(!ray.intersect(bbox)){
        return false;
    } else if(isLeaf()){
        return ray.hasHit(triangles);
    }

    Vec3Df hitMin, hitMax;
    r.intersect(leftTree->bbox, hitMin);
    r.intersect(rightTree->bbox, hitMax);

    if(hitMin && !hitMax){
        return leftTree->hasHit(r);
    } else if(!hitMin && hitMax){
        return rightTree->hasHit(r);
    }

    return leftTree->hasHit(r) ||
            rightTree->hasHit(r) ;
}

bool KdTree::searchHit(Ray & ray, Vertex & hit, float & distance){

    if(this==NULL){
        return false;
    } else if(!ray.intersect(bbox)){
        return false;
    } else if(isLeaf()){
        return ray.intersectVecteurDeTriangles_v(mesh, triangles, hit, distance);
    }

    Vec3Df hitMin, hitMax;
    r.intersect(leftTree->bbox, hitMin);
    r.intersect(rightTree->bbox, hitMax);


    if(hitMin && !hitMax){
        return leftTree->searchHit(ray, hit, distance);
    } else if(!hitMin && hitMax){
        return rightTree->searchHit(ray, hit, distance);
    }

    return leftTree->searchHit(ray, hit, distance) ||
            rightTree->searchHit(ray, hit, distance) ;
}
