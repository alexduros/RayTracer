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

#define DEFAULT_RAY_PER_LIGHT 10

#define REGULAR_ANTI_ALIASING 0
#define STOCHASTIC_ANTI_ALIASING 1

class RayTracer {

    public:
        static RayTracer * getInstance ();
        static void destroyInstance ();

        inline const Vec3Df & getBackgroundColor () const { return backgroundColor; }
        inline void setBackgroundColor (const Vec3Df & c) { backgroundColor = c; }
        inline void setShadowMode(const bool & mode) { shadowMode = mode; }
        inline void setAmbientOcclusion(const bool & ao){ambientOcclusion = ao; }
        inline void setAntiAliasing(const bool & aa){antialiasing = aa; }
        inline void setAliasingMode(const int & mode){this->mode = mode; }
        inline void setRayPerLight(const int & rpl){rayPerLight = rpl; }
        inline void setNumDir(const int & numdir){numDir = numdir; }

        void buildKDTrees(Scene * scene, vector<KdTree *> & kdtrees);
        void searchKDTreeHit(std::vector<KdTree *> kdTrees, const Ray & ray, Scene * scene, unsigned int & kdTree);

        Vec3Df rayTrace(vector<KdTree*> & kdTrees, Scene * scene, Ray & r);
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
            rayPerLight = DEFAULT_RAY_PER_LIGHT;
            numDir = 10;
            mode = 0;
        }
        inline virtual ~RayTracer () {}

    private:
        Vec3Df backgroundColor;
        bool shadowMode;
        bool ambientOcclusion;
        bool antialiasing;
        unsigned int rayPerLight;
        unsigned int numDir;
        unsigned int mode;
};

#endif // RAYTRACER_H

// Some Emacs-Hints -- please don't remove:
//
//  Local Variables:
//  mode:C++
//  tab-width:4
//  End:
