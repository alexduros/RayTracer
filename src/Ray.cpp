// *********************************************************
// Ray Class
// Author : Tamy Boubekeur (boubek@gmail.com).
// Copyright (C) 2010 Tamy Boubekeur.
// All rights reserved.
// *********************************************************

#include "Ray.h"

using namespace std;

static const unsigned int NUMDIM = 3, RIGHT = 0, LEFT = 1, MIDDLE = 2;

bool Ray::hit (const Triangle & triangle, const Mesh & mesh, Vertex & hit){
    Vec3Df uvw;
    const vector<Vertex> & vertices = mesh.getVertices();
    const Vec3Df & A = vertices[triangle.getVertex(0)].getPos(),
            & B = vertices[triangle.getVertex(1)].getPos(),
            & C = vertices[triangle.getVertex(2)].getPos();

    const Vec3Df & An = vertices[triangle.getVertex(0)].getNormal(),
            & Bn = vertices[triangle.getVertex(1)].getNormal(),
            & Cn = vertices[triangle.getVertex(2)].getNormal();

    float Ac = vertices[triangle.getVertex(0)].getAmbientOcclusionCoeff();
    float Bc = vertices[triangle.getVertex(1)].getAmbientOcclusionCoeff();
    float Cc = vertices[triangle.getVertex(2)].getAmbientOcclusionCoeff();

    Vec3Df normal = Vec3Df::crossProduct(B - A, C - A);
    normal.normalize();

    if (Vec3Df::dotProduct(normal, direction) > 0)
        return false;

    float distance = -Vec3Df::dotProduct(origin - A, normal) / Vec3Df::dotProduct(direction, normal);

    if (distance <= 0)
        return false;

    Vec3Df P = origin + direction * distance;
    Vec3Df W = P - A,
            U = B - A,
            V = C - A;
    Vec3Df NU = Vec3Df::crossProduct(normal, U),
            NV = Vec3Df::crossProduct(normal, V);

    float u = (Vec3Df::dotProduct(W, NU)) / (Vec3Df::dotProduct(V, NU)),
            v = (Vec3Df::dotProduct(W, NV)) / (Vec3Df::dotProduct(U, NV));

    if (u >= 0 && v >= 0 && (u + v <= 1)) {
        uvw[2] = u;
        uvw[1] = v;
        uvw[0] = 1 - u - v;
        hit.setPos(A * uvw[0] + B * uvw[1] + C * uvw[2]);
        hit.setNormal(An * uvw[0] + Bn * uvw[1] + Cn * uvw[2]);
        hit.setAmbientOcclusionCoeff((Ac * uvw[0] + Bc * uvw[1] + Cc * uvw[2]));

        return true;
    } else {
        return false;
    }
}

bool Ray::nearestHit (const Mesh & mesh, Vertex & hit , float & distance){
    bool hasHit = false;
    for(unsigned int i=0;i<mesh.getTriangles().size();i++){
        if(this->hit(mesh.getTriangles()[i], mesh, hit)){
            if(!distance || Vec3Df::squaredDistance(origin, hit.getPos()) < distance){
                distance = Vec3Df::squaredDistance(origin, hit.getPos());
                hasHit = true;
            }
        }
    }
    return hasHit;
}
