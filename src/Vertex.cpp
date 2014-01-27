// *********************************************************
// Vertex Class
// Author : Tamy Boubekeur (boubek@gmail.com).
// Copyright (C) 2008 Tamy Boubekeur.
// All rights reserved.
// *********************************************************

#include "Vertex.h"

#include <cmath>
#include <algorithm>

using namespace std;

static const unsigned int SIZE_OF_VERTEX = 10;

ostream & operator<< (ostream & output, const Vertex & v) {
    output << v.getPos () << endl << v.getNormal ();
    return output;
}

void Vertex::interpolate (const Vertex & u, const Vertex & v, float alpha) {
    setPos (Vec3Df::interpolate (u.getPos (), v.getPos (), alpha));
    Vec3Df normal = Vec3Df::interpolate (u.getNormal (), v.getNormal (), alpha);
    normal.normalize ();
    setNormal (normal);
}

// ------------------------------------
// Static Members Methods.
// ------------------------------------

void Vertex::computeAveragePosAndRadius (std::vector<Vertex> & vertices,
                                         Vec3Df & center, float & radius) {
    center = Vec3Df (0.0, 0.0, 0.0);
    for (unsigned int i = 0; i < vertices.size (); i++)
        center += vertices[i].getPos ();
    center /= float (vertices.size ());
    radius = 0.0;
    for (unsigned int i = 0; i < vertices.size (); i++) {
        float vDistance = Vec3Df::distance (center, vertices[i].getPos ());
        if (vDistance > radius)
            radius = vDistance;
    }
}

void Vertex::scaleToUnitBox (vector<Vertex> & vertices,
                             Vec3Df & center, float & scaleToUnit) {
    computeAveragePosAndRadius (vertices, center, scaleToUnit);
    for (unsigned int i = 0; i < vertices.size (); i++)
        vertices[i].setPos (Vec3Df::segment (center, vertices[i].getPos ()) / scaleToUnit);
}

void Vertex::normalizeNormals (vector<Vertex> & vertices) {
    for (std::vector<Vertex>::iterator it = vertices.begin ();
    it != vertices.end ();
    it++) {
        Vec3Df n = it->getNormal ();
        if (n != Vec3Df (0.0, 0.0, 0.0)) {
            n.normalize ();
            it->setNormal (n);
        }
    }
}

inline bool sortX(Vertex v1, Vertex v2){
    return v1.getPos()[0] < v2.getPos()[0] ;
}

inline bool sortY(Vertex v1, Vertex v2){
    return v1.getPos()[1] < v2.getPos()[1] ;
}

inline bool sortZ(Vertex v1, Vertex v2){
    return v1.getPos()[2] < v2.getPos()[2] ;
}

void Vertex::sortByDirection(std::vector<Vertex> & vertices, int direction){
    switch(direction){
        case 0:
            sort (vertices.begin(), vertices.end(), sortX);
            break;
        case 1:
            sort (vertices.begin(), vertices.end(), sortY);
            break;
        case 3:
            sort (vertices.begin(), vertices.end(), sortZ);
            break;
    }
}

const Vertex & Vertex::getMedian(std::vector<Vertex> & vertices, int direction){
    Vertex::sortByDirection(vertices, direction);
    return vertices[vertices.size()/2] ;
}



