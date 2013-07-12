// *********************************************************
// Ray Tracer Class
// Author : Tamy Boubekeur (boubek@gmail.com).
// Copyright (C) 2010 Tamy Boubekeur.
// All rights reserved.
// *********************************************************

#ifndef RAYTRACER_H
#define RAYTRACER_H

#include <iostream>
#include <vector>
#include <QImage>

#include "Vec3D.h"
#include "KdTree.h"
#include "Scene.h"

class RayTracer {

    public:
        static RayTracer * getInstance ();
        static void destroyInstance ();

        inline const Vec3Df & getBackgroundColor () const { return backgroundColor;}
        inline void setBackgroundColor (const Vec3Df & c) { backgroundColor = c; }
        inline void setShadowMode(const bool & mode) { shadowMode = mode;}
        inline void setAmbientOcclusion(const bool & ao){ambientOcclusion = ao;}
        inline void setAntiAliasing(const bool & aa){antialiasing = aa;}
        inline void setAliasingMode(const int & mode){this->mode = mode;}
        inline void setRayPerLight(const int & rpl){rayPerLight = rpl;}
        inline void setNumDir(const int & numdir){numDir = numdir;}

        void creerArbres(Scene * scene, vector<KdTree *> & arbresScene);
        bool recParcoursArbre_v(KdTree * n, Ray & r, const Object & object, Vertex & intersectionPoint, float & distance);
        bool recParcoursArbreExistence_v(KdTree * n, Ray & r, const Object & object);
        bool ParcoursScene_v(vector<KdTree *> arbresScenes, Ray & r, Scene * s, Vertex & intersectionPoint, int & numObject);
        bool ParcoursSceneExistence_v(vector<KdTree *> arbresScenes, Ray & r, Scene * s);

        Vec3Df lancerDeRayon(vector<KdTree*> & arbresScene, Scene * scene, Ray & r );
        QImage render (const Vec3Df & camPos,
                       const Vec3Df & viewDirection,
                       const Vec3Df & upVector,
                       const Vec3Df & rightVector,
                       float fieldOfView,
                       float aspectRatio,
                       unsigned int screenWidth,
                       unsigned int screenHeight);

    protected:
        inline RayTracer () {
            this->shadowMode = false;
            this->ambientOcclusion = false;
            this->antialiasing = false;
            rayPerLight = 10;
            numDir = 10;
            mode = 0;
        }
        inline virtual ~RayTracer () {}

    private:
        Vec3Df backgroundColor;
        bool shadowMode;
        bool ambientOcclusion;
        bool antialiasing;
        int rayPerLight;
        int numDir;
        int mode;
};

#endif // RAYTRACER_H

// Some Emacs-Hints -- please don't remove:
//
//  Local Variables:
//  mode:C++
//  tab-width:4
//  End:
