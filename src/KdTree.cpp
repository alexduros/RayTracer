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
        isLeaf = false;

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
            Mesh * leftMesh = new Mesh(leftTriangles, leftVertices);
            leftTree = KdTree(&leftMesh, maxDepth + 1, step);
        }
        if(rightVertices.size() !=0 && rightTriangles.size() != 0){
            Mesh * rightMesh = new Mesh(rightTriangles, rightVertices);
            rightTree = KdTree(&rightMesh, maxDepth + 1, step);
        }
    } else {
        isLeaf = true;
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

//TODO
bool KdTree::recParcoursArbreExistence_v(Ray & r, const Mesh & mesh){

    Vec3Df neSertaRien;

    //Cas triviaux
    if(this==NULL){
        return false;
    }
    if(!r.intersect(this->bbox_n,neSertaRien)){
        return false;
    }
    if(this->isFeuille){
        //donne juste l'existence (plus rapide)
        return r.existeIntersectionVecteur(mesh, this->triangles_n);

    }

    else{
        //on test l'intersection avec la boite englobante des sous arbres
        Vec3Df t_split_min, t_split_max;
        bool intersect_Tg = r.intersect(this->bbox_n, t_split_min);
        bool intersect_Td = r.intersect(this->bbox_n, t_split_max);

        if(intersect_Tg && !intersect_Td){
            return this->Tg->recParcoursArbreExistence_v( r, mesh);
        }
        else if(!intersect_Tg && intersect_Td){
            return this->Td->recParcoursArbreExistence_v( r, mesh);
        }
        else{

            //seul le premier est calculé si il trouve une intersection
            return this->Td->recParcoursArbreExistence_v( r, mesh) ||
                    this->Tg->recParcoursArbreExistence_v( r, mesh) ;
        }
    }



}

bool KdTree::recParcoursArbre_v(Ray & r, const Mesh & mesh, Vertex & intersectionPoint, float & distance){
    bool resultat_parcours_gauche;
    bool resultat_parcours_droit;
    Vec3Df neSertaRien;

    //Cas triviaux
    if(this==NULL){
        return false;
    }
    if(!r.intersect(this->bbox_n,neSertaRien)){
        return false;
    }
    if(this->isFeuille){
        //retourne le point d'intersection le plus proche pour le vecteur de triangles !
        //met à jour la valeur de distance;
        return r.intersectVecteurDeTriangles_v(mesh, this->triangles_n, intersectionPoint, distance);

    }



    else{
        //on test l'intersection avec la boite englobante des sous arbres
        Vec3Df t_split_min, t_split_max;
        bool intersect_Tg = r.intersect(this->bbox_n, t_split_min);
        bool intersect_Td = r.intersect(this->bbox_n, t_split_max);

        if(intersect_Tg && !intersect_Td){
            return this->Tg->recParcoursArbre_v( r, mesh, intersectionPoint, distance);
        }
        else if(!intersect_Tg && intersect_Td){
            return this->Td->recParcoursArbre_v( r, mesh, intersectionPoint, distance);
        }
        else{
            resultat_parcours_droit = this->Td->recParcoursArbre_v( r, mesh, intersectionPoint, distance);
            resultat_parcours_gauche = this->Tg->recParcoursArbre_v( r, mesh, intersectionPoint, distance);

            return resultat_parcours_droit || resultat_parcours_gauche ;
        }
    }
}


