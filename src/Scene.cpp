// *********************************************************
// Scene Class
// Author : Tamy Boubekeur (boubek@gmail.com).
// Copyright (C) 2010 Tamy Boubekeur.
// All rights reserved.
// *********************************************************

#include <fstream>
#include "Scene.h"

Vec3Df EPS = Vec3Df(0.001,0.001,0.001);
using namespace std;

static Scene * instance = NULL;

Scene * Scene::getInstance () {
    if (instance == NULL)
    {
        cout << "creating new scene" << endl;
        instance = new Scene ();
    }
    return instance;
}

void Scene::destroyInstance () {
    if (instance != NULL) {
        delete instance;
        instance = NULL;
    }
}

Scene::Scene () {
    buildDefaultScene (true);
    updateBoundingBox ();
}

Scene::~Scene () {
}

void Scene::updateBoundingBox () {
    cout << "updating bounding box" << endl;
    if (objects.empty ())
        bbox = BoundingBox ();
    else {
        bbox = objects[0].getBoundingBox ();
        for (unsigned int i = 1; i < objects.size (); i++)
            bbox.extendTo (objects[i].getBoundingBox ());
    }
}

// Changer ce code pour créer des scènes originales
void Scene::buildDefaultScene (bool HD) {
    // Mesh groundMesh;
    // if (HD)
    //     groundMesh.loadOFF ("models/ground_HD.off");
    // else
    //     groundMesh.loadOFF ("models/ground.off");
    // Material groundMat;
    // Object ground (groundMesh, groundMat);
    // objects.push_back (ground);
    cout << "load mesh" << endl;
    Mesh ramMesh;
    ramMesh.loadOFF ("/Users/alexandre/dev/RayTracer/src/models/minion.off");
    Material ramMat (1.f, 1.f, Vec3Df (1.f, .6f, .2f));
    KdTree kdTree (ramMesh, 0, 300);
    // kdTree.build();
    Object ram (ramMesh, ramMat, kdTree);
    objects.push_back (ram);
    Light l (Vec3Df (3.0f, 3.0f, 3.0f), Vec3Df (0.0f, 1.0f, 1.0f), 1.0f, 3.0f);
    lights.push_back (l);
    Light l1 (Vec3Df (-2.0f, -2.0f, 2.0f), Vec3Df (1.0f, 1.0f, 0.0f), 0.5f, 3.0f);
    lights.push_back (l1);
    Light l2 (Vec3Df (0.0f, -2.0f, 2.0f), Vec3Df (1.0f, 1.0f, 1.0f), 0.8f, 3.0f);
    lights.push_back (l2);
}

void Scene::calculAmbientOcclusion(){
    //Paramètres à tester
    unsigned int nbDirections = 500;

    //AO.resize(mesh.V.size(),0);
    int nbObject = this->getObjects().size();
    Vertex intersectionPoint;

    cout<<"debut calcul AO"<<endl;
    for(int k=0;k<nbObject;k++){

        Mesh mesh = this->getObjects()[k].getMesh();
        KdTree kdTree = this->getObjects()[k].getKdTree();
        vector<float> AO(mesh.getVertices().size());
        cout<<"object numero "<<k<<endl;

        for(unsigned int i=0;i<mesh.getVertices().size();i++){
            for(unsigned int j=0;j<nbDirections;j++){
                float rayon_sphere = 0.5;
                //on tire un nouveau rayon aléatoire
                Vec3Df dir = Vec3Df(rand(), rand(), rand());
                Ray r = Ray(mesh.getVertices()[i].getPos()+EPS, dir );


                //!! ce n'est pas tout a fait ça il faut regarder la scene et pas seulement l'arbre de l'object
                if(kdTree.searchHit(r, intersectionPoint, rayon_sphere)){
                    //cout<<"test intersection réussi"<<endl;
                    AO[i] += 1.0;
                    float a = mesh.getVertices()[i].getAmbientOcclusionCoeff() + 1;
                    mesh.getVertices()[i].setAmbientOcclusionCoeff(a);

                }


            } //fin pour toutes les directions

            float b = mesh.getVertices()[i].getAmbientOcclusionCoeff() / (float)nbDirections;
            mesh.getVertices()[i].setAmbientOcclusionCoeff(b);
            AO[i]/=(float)nbDirections;
            //cout<<"vertex numero "<<i<<"/"<<mesh.getVertices().size()<<" mis à jour avec la valeur = "<<b<<endl;
            this->setObjectAO(k,i,b);
        }//fin pour tous les vertex

    }

    cout<<"calcul AO terminé"<<endl;

};

