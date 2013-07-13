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

bool Ray::hit (const Mesh & mesh, Vertex & hit , float & distance){
    bool hasHit = false;

    for(int i=0;i<mesh.getTriangles().size();i++){
        if(hit(mesh.getTriangles()[i], mesh, hit)){
            resultat = true;
            if(Vec3Df::squaredDistance(origin, hit.getPos()) < distance){
                distance = Vec3Df::squaredDistance(origin, hit.getPos());
                hasHit = true;
            }
        }
    }

    return hasHit;
}


bool Ray::hasHit (const Mesh & mesh, const std::vector<Triangle> & triangles){
    triangles.size();
    Vertex intersectionPointTemp;
    if(triangles.size() == 0){ return false; }
    for(int i=0;i<triangles.size();i++){
        if(intersect_ray_triangle_v (triangles[i], mesh, intersectionPointTemp)){
            return true;
        }
    }

    return false;
}

bool Ray::intersect (const BoundingBox & bbox, Vec3Df & intersectionPoint) const {
    const Vec3Df & minBb = bbox.getMin ();
    const Vec3Df & maxBb = bbox.getMax ();
    bool inside = true;
    unsigned int  quadrant[NUMDIM];
    register unsigned int i;
    unsigned int whichPlane;
    Vec3Df maxT;
    Vec3Df candidatePlane;

    for (i=0; i<NUMDIM; i++)
        if (origin[i] < minBb[i]) {
        quadrant[i] = LEFT;
        candidatePlane[i] = minBb[i];
        inside = false;
    } else if (origin[i] > maxBb[i]) {
        quadrant[i] = RIGHT;
        candidatePlane[i] = maxBb[i];
        inside = false;
    } else	{
        quadrant[i] = MIDDLE;
    }

    if (inside)	{
        intersectionPoint = origin;
        return (true);
    }

    for (i = 0; i < NUMDIM; i++)
        if (quadrant[i] != MIDDLE && direction[i] !=0.)
            maxT[i] = (candidatePlane[i]-origin[i]) / direction[i];
    else
        maxT[i] = -1.;

    whichPlane = 0;
    for (i = 1; i < NUMDIM; i++)
        if (maxT[whichPlane] < maxT[i])
            whichPlane = i;

    if (maxT[whichPlane] < 0.) return (false);
    for (i = 0; i < NUMDIM; i++)
        if (whichPlane != i) {
        intersectionPoint[i] = origin[i] + maxT[whichPlane] *direction[i];
        if (intersectionPoint[i] < minBb[i] || intersectionPoint[i] > maxBb[i])
            return (false);
    } else {
        intersectionPoint[i] = candidatePlane[i];
    }
    return (true);
};
