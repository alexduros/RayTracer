// Copyright (C) 2013 Alexandre Duros.
// All rights reserved.
// ---------------------------------------------------------


#include "KdTree.h"
#include "Vertex.h"
#include "Triangle.h"

using namespace std;

void KdTree::build () {
    const vector<Triangle> & triangles = mesh.getTriangles();
    vector<Vertex> & vertices = mesh.getVertices();
    bbox = BoundingBox::computeBoundingBoxTriangles(triangles, mesh);

    if(step > triangles.size() && maxDepth <= MAX_DEPTH){
        leaf = false;

        int direction = bbox.getDirection();
        Vertex::getMedian(vertices, direction);
        Vertex::sortByDirection(vertices, direction);
        Mesh leftMesh, rightMesh;
        KdTree leftTree, rightTree;
        mesh.split(direction, rightMesh, leftMesh);
        leftTree = KdTree(leftMesh, maxDepth + 1, step);
        rightTree = KdTree(rightMesh, maxDepth + 1, step);
    } else {
        leaf = true;
    }

}

void KdTree::recDrawBoundingBox(unsigned int depth){
    if(maxDepth == depth){
        bbox.renderGL();
    } else {
        rightTree->recDrawBoundingBox(depth);
        leftTree->recDrawBoundingBox(depth);
    }
}

void KdTree::renderGL (unsigned int depth) const {
    if (maxDepth <= depth) {
        bbox.renderGL();
    } else if(leftTree != NULL && rightTree != NULL) {
        leftTree->renderGL(depth);
        rightTree->renderGL(depth);
    } else if(leftTree == NULL && rightTree == NULL){
        bbox.renderGL();
    }
}

bool KdTree::hasHit(const Ray & ray){
    Ray theRay = ray;

    if(this==NULL){
        return false;
    } else if(!ray.intersect(bbox)){
        return false;
    } else if(isLeaf()){
        return theRay.hasHit(mesh);
    }

    Vec3Df hitMin, hitMax;
    ray.intersect(leftTree->bbox, hitMin);
    ray.intersect(rightTree->bbox, hitMax);

    if(hitMin.getLength() > 0 && hitMax.getLength() == 0){
        return leftTree->hasHit(ray);
    } else if(hitMin.getLength() == 0 && hitMax.getLength() > 0){
        return rightTree->hasHit(ray);
    }

    return leftTree->hasHit(ray) || rightTree->hasHit(ray) ;
}

bool KdTree::searchHit(const Ray & ray, Vertex & hit, float & distance){
    Ray theRay;

    if(this==NULL){
        return false;
    } else if(!ray.intersect(bbox)){
        return false;
    } else if(isLeaf()){
        return theRay.nearestHit(mesh, hit, distance);
    }

    Vec3Df hitMin, hitMax;
    ray.intersect(leftTree->bbox, hitMin);
    ray.intersect(rightTree->bbox, hitMax);


    if(hitMin.getLength() > 0 && hitMax.getLength() == 0){
        return leftTree->searchHit(ray, hit, distance);
    } else if(hitMin.getLength() == 0 && hitMax.getLength() > 0){
        return rightTree->searchHit(ray, hit, distance);
    }

    return leftTree->searchHit(ray, hit, distance) || rightTree->searchHit(ray, hit, distance) ;
}
