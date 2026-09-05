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

#include "Vec3D.h"
#include "KdTree.h"
#include "Scene.h"
#include "Image.h"

#define DEFAULT_RAY_PER_LIGHT 10

#define REGULAR_ANTI_ALIASING 0
#define STOCHASTIC_ANTI_ALIASING 1

class RayTracer {

    public:
        // Debug visualization modes. LIT is the real shading path (currently
        // mostly stubbed); the others bypass shading and colorize the hit
        // information directly so we can verify that rays, intersections,
        // and normals are behaving before fixing the lit path.
        enum class DebugMode {
            LIT,          // existing ambient-only path (very dark on purpose)
            AMBIENT,      // flat material color, no 0.33 multiplier
            HIT_MASK,     // white = hit, black = miss
            NORMALS,      // (n + 1) / 2 as RGB
            DEPTH,        // hit distance normalized, blue→red
            OBJECT_ID     // distinct color per object index
        };

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
        inline void setDebugMode(DebugMode m){ debugMode = m; }
        inline DebugMode getDebugMode() const { return debugMode; }
        inline void setDepthRange(float maxDist){ depthRange = maxDist; }

        void buildKDTrees(Scene * scene, vector<KdTree *> & kdtrees);
        // Returns the hit object index (UINT_MAX on miss), along with the hit
        // vertex and distance for the *closest* object in the list.
        void searchKDTreeHit(std::vector<KdTree *> kdTrees, const Ray & ray, Scene * scene,
                             unsigned int & kdTree, Vertex & hitOut, float & distanceOut);

        Vec3Df rayTrace(vector<KdTree*> & kdTrees, Scene * scene, Ray & r);
        Image render (const Vec3Df & camPos,
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
            debugMode = DebugMode::AMBIENT;
            depthRange = 10.f;
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
        DebugMode debugMode;
        float depthRange;

    public:
        // Per-render stats, written by rayTrace(), read after render() returns.
        unsigned long statsHits = 0;
        unsigned long statsMisses = 0;
        float statsMinHitDist = 0.f;
        float statsMaxHitDist = 0.f;
};

#endif // RAYTRACER_H

// Some Emacs-Hints -- please don't remove:
//
//  Local Variables:
//  mode:C++
//  tab-width:4
//  End:
