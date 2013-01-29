// *********************************************************
// Ray Class
// Author : Tamy Boubekeur (boubek@gmail.com).
// Copyright (C) 2010 Tamy Boubekeur.
// All rights reserved.
// *********************************************************

#include "Ray.h"

using namespace std;

static const unsigned int NUMDIM = 3, RIGHT = 0, LEFT = 1, MIDDLE = 2;


///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	intersection avec la normale
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool Ray::intersect_ray_triangle_v (const Triangle & triangle, const Mesh & mesh, Vertex & intersectionPoint){
    const vector<Vertex> & vertices = mesh.getVertices();
    Vec3Df uvw;
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
        intersectionPoint.setPos(A * uvw[0] + B * uvw[1] + C * uvw[2]);
        intersectionPoint.setNormal(An * uvw[0] + Bn * uvw[1] + Cn * uvw[2]);
        intersectionPoint.setAmbientOcclusionCoeff((Ac * uvw[0] + Bc * uvw[1] + Cc * uvw[2]));

        return true;
    }
    else
        return false;

}

bool Ray::intersect_v (const Mesh & mesh, Vertex & intersectionPoint , float distance){
    int nb_triangles = mesh.getTriangles().size();
    Vertex intersectionPointTemp = Vertex();
    float distance_min_cam = distance;
    bool resultat = false;

    //on parcourt tout le mesh
    for(int i=0;i<nb_triangles;i++){
        if(intersect_ray_triangle_v (mesh.getTriangles()[i], mesh, intersectionPointTemp)){

            resultat = true;
            //Test intersection plus proche
            if(Vec3Df::squaredDistance(origin, intersectionPointTemp.getPos()) < distance_min_cam ){
                distance_min_cam = Vec3Df::squaredDistance(origin, intersectionPointTemp.getPos());
                intersectionPoint = intersectionPointTemp ;

            }

        }
    }
    //cout<<"resultat ="<<resultat<<endl;
    distance = distance_min_cam ;
    return resultat;
}


//Donne l'existence de l'intersection et l'intersection la plus proche de la caméra
bool Ray::intersectVecteurDeTriangles_v (const Mesh & mesh, const std::vector<Triangle> & triangles, Vertex & intersectionPoint, float & distance){

    int nb_triangles = triangles.size();
    Vertex intersectionPointTemp = Vertex();
    float distance_min_cam = distance;
    bool resultat = false;

    if(triangles.size()==0){ return false;}
    //on parcourt tout le mesh
    for(int i=0;i<nb_triangles;i++){

        if(intersect_ray_triangle_v (triangles[i], mesh, intersectionPointTemp)){

            resultat = true;
            //Test intersection plus proche
            if(Vec3Df::squaredDistance(origin, intersectionPointTemp.getPos()) < distance_min_cam ){
                distance_min_cam = Vec3Df::squaredDistance(origin, intersectionPointTemp.getPos());
                intersectionPoint = intersectionPointTemp ;

            }

        }
    }

    distance = distance_min_cam; //VERIFIER LE PASSAGE MODIFIE LA VALEUR EXTERIEUR ET QU IL NE FAUT PAS UN &
    return resultat;
};


//Donne juste l'existence (beaucoup plus rapide)
bool Ray::existeIntersectionVecteur (const Mesh & mesh, const std::vector<Triangle> & triangles){
    int nb_triangles = triangles.size();
    Vertex intersectionPointTemp;
    if(triangles.size()==0){ return false;}
    //on parcourt tout le mesh
    for(int i=0;i<nb_triangles;i++){

        if(intersect_ray_triangle_v (triangles[i], mesh, intersectionPointTemp)){

            return true;
        }
    }

    return false;
};

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
