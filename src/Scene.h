// *********************************************************
// Scene Class
// Author : Tamy Boubekeur (boubek@gmail.com).
// Copyright (C) 2010 Tamy Boubekeur.
// All rights reserved.
// *********************************************************

#ifndef SCENE_H
#define SCENE_H

#include <iostream>
#include <vector>

#include "Object.h"
#include "Light.h"
#include "BoundingBox.h"
#include "Ray.h"
#include "KdTree.h"

class Scene {
    public:
        static Scene * getInstance ();
        static void destroyInstance ();

        inline std::vector<Object> & getObjects () { return objects; }
        inline const std::vector<Object> & getObjects () const { return objects; }

        inline std::vector<Light> & getLights () { return lights; }
        inline const std::vector<Light> & getLights () const { return lights; }

        inline const BoundingBox & getBoundingBox () const { return bbox; }
        void updateBoundingBox ();
        void calculAmbientOcclusion();

        void setObjectAO(int numObject, int numVertex, float ao){
            objects[numObject].setMeshAO(numVertex,ao);
        }

        void updateAO(int numObject, Mesh mesh){
            objects[numObject].updateMesh(mesh);
        };

        void parcoursScene(Ray r , Vertex & intersectionPoint, float & distance){
            for(unsigned int i = 0;i<objects.size();i++){
                objects[i].parcoursObject(r, intersectionPoint, distance);
            }
        }

        inline QString getOFFFilename () const { return offFilename; }
        void setOFFFilename (const QString & filename);

    protected:
        Scene ();
        virtual ~Scene ();

    private:
        QString offFilename;
        void buildDefaultScene ();
        std::vector<Object> objects;
        std::vector<Light> lights;
        BoundingBox bbox;
};


#endif // SCENE_H

// Some Emacs-Hints -- please don't remove:
//
//  Local Variables:
//  mode:C++
//  tab-width:4
//  End:
