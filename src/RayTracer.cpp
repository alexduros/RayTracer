// *********************************************************
// Ray Tracer Class
// Author : Tamy Boubekeur (boubek@gmail.com).
// Copyright (C) 2010 Tamy Boubekeur.
// All rights reserved.
// *********************************************************

#include "RayTracer.h"
#include "Ray.h"
#include "Scene.h"
#include "KdTree.h"
#include "Image.h"
#include <climits>
#include <limits>
#include <cmath>

const Vec3Df BLACK = Vec3Df(0.000f, 0.000f, 0.000f);
const Vec3Df EPS = Vec3Df(0.001, 0.001, 0.001);
static RayTracer * instance = nullptr;

using namespace std;

RayTracer * RayTracer::getInstance () {
    if (instance == nullptr)
        instance = new RayTracer ();
    return instance;
}

void RayTracer::destroyInstance () {
    if (instance != nullptr) {
        delete instance;
        instance = nullptr;
    }
}

inline int clamp (float f, int inf, int sup) {
    int v = static_cast<int> (f);
    return (v < inf ? inf : (v > sup ? sup : v));
}

Image RayTracer::render (const Vec3Df & camPos,
                         const Vec3Df & direction,
                         const Vec3Df & upVector,
                         const Vec3Df & rightVector,
                         float fieldOfView,
                         float aspectRatio,
                         unsigned int screenWidth,
                         unsigned int screenHeight) {
    Image image(screenWidth, screenHeight, Image::RGB888);

    clock_t start, finish;
    cout << "Rendering started..." << endl;
    start = clock();

    Scene * scene = Scene::getInstance();
    vector<KdTree *> kdTrees;

    buildKDTrees(scene, kdTrees);

    if(ambientOcclusion){
        scene->calculAmbientOcclusion();
    }

    // Reset per-render stats. rayTrace() updates these on every ray; printed
    // below so one line per render tells us: "did the rays actually hit?".
    statsHits = 0;
    statsMisses = 0;
    statsMinHitDist = 0.f;
    statsMaxHitDist = 0.f;
    cout << "  camPos=(" << camPos[0] << "," << camPos[1] << "," << camPos[2] << ")"
         << " dir=(" << direction[0] << "," << direction[1] << "," << direction[2] << ")"
         << " fov=" << fieldOfView
         << " objects=" << scene->getObjects().size() << endl;

    for (unsigned int i = 0; i < screenWidth; i++){
        for (unsigned int j = 0; j < screenHeight; j++) {
            float tanX = tan (fieldOfView);
            float tanY = tanX/aspectRatio;
            Vec3Df stepX = (float (i) - screenWidth/2.f)/screenWidth * tanX * rightVector;
            Vec3Df stepY = (float (j) - screenHeight/2.f)/screenHeight * tanY * upVector;
            Vec3Df step = stepX + stepY;
            Vec3Df dir = direction + step;
            dir.normalize ();
            //Rayon
            Ray ray (camPos, dir);

            //Intersection Rayon scene, couleur
            Vec3Df hit, color;

            if(antialiasing){
                if(mode == REGULAR_ANTI_ALIASING){
                    float coeff =1;
                    for (unsigned int a=0; a<numDir; a++) {
                        for(unsigned int b=0; b<numDir; b++) {
                            Vec3Df stepX = (float (i) - screenWidth/2.f + a*coeff)/screenWidth * tanX * rightVector;
                            Vec3Df stepY = (float (j) - screenHeight/2.f + b*coeff)/screenHeight * tanY * upVector;
                            Vec3Df dir = direction + stepX + stepY;
                            dir.normalize();
                            Ray ray(camPos, dir);
                            color += rayTrace(kdTrees, scene, ray);
                            color /=(numDir*numDir);
                        }
                    }
                }
                if(mode == STOCHASTIC_ANTI_ALIASING){
                    for (unsigned int a = 0; a<numDir; a++) {
                        Vec3Df stepX = (float (i) - screenWidth/2.f + rand()/(float(RAND_MAX)+1))/screenWidth * tanX * rightVector;
                        Vec3Df stepY = (float (j) - screenHeight/2.f + rand()/(float(RAND_MAX)+1))/screenHeight * tanY * upVector;
                        Vec3Df dir = direction + stepX + stepY;
                        dir.normalize();
                        Ray ray(camPos, dir);
                        color += rayTrace(kdTrees, scene, ray);
                    }
                    color /= numDir;
                }
            }

            color = RayTracer::rayTrace(kdTrees, scene, ray);

            image.setPixel (i, ((screenHeight-1)-j),
                            clamp (color[0], 0, 255),
                            clamp (color[1], 0, 255),
                            clamp (color[2], 0, 255));

        }
    }

    finish = clock();
    cout << "Rendering finished after " << ((float)(finish - start) / CLOCKS_PER_SEC) << " seconds" << endl;
    const unsigned long totalRays = statsHits + statsMisses;
    const float hitPct = totalRays ? (100.f * statsHits / totalRays) : 0.f;
    cout << "  rays=" << totalRays
         << " hits=" << statsHits << " (" << hitPct << "%)"
         << " misses=" << statsMisses
         << " hitDist=[" << statsMinHitDist << "," << statsMaxHitDist << "]"
         << endl;
    return image;
}

void RayTracer::searchKDTreeHit(std::vector<KdTree *> kdTrees, const Ray & ray, Scene * scene,
                                unsigned int & kdTree, Vertex & hitOut, float & distanceOut) {
    // NOTE: brute-force over triangles for now. The KdTree::searchHit path has
    // two latent bugs (default-constructed Ray used at the leaf; root bbox
    // never initialized because build() is skipped), so we go direct until
    // the acceleration structure is repaired.
    (void)kdTrees;
    kdTree = UINT_MAX;
    distanceOut = std::numeric_limits<float>::max();
    auto & objects = scene->getObjects();
    for (unsigned int i = 0; i < objects.size(); ++i) {
        Vertex hit;
        float dist = 0.f;  // Ray::nearestHit treats 0 as "no hit yet"
        Ray localRay = ray;
        if (localRay.nearestHit(objects[i].getMesh(), hit, dist)) {
            // dist is a *squared* distance (see Ray::nearestHit).
            if (dist < distanceOut) {
                distanceOut = dist;
                hitOut = hit;
                kdTree = i;
            }
        }
    }
}

void RayTracer::buildKDTrees(Scene * scene, vector<KdTree *> & kdtrees){
    for(unsigned int i=0; i<scene->getObjects().size(); i++){
        KdTree * kdtree = new KdTree(scene->getObjects()[i].getMesh(), 0, 300);
        kdtrees.push_back(kdtree);
    }
}

Vec3Df RayTracer::rayTrace(vector<KdTree*> & kdTrees, Scene * scene, Ray & ray){
    Vec3Df couleurDiffus    = BLACK,
           couleurAmbient   = BLACK,
           couleurSpecular  = BLACK;
//           couleurTransmis  = BLACK,
//           couleurReflechis = BLACK;

//    Ray rayTransmis, rayReflechis, rayDiffus;
    // Sentinel: UINT_MAX = "no object hit". Using 0 as the miss value would
    // mis-treat a hit on object 0 as a miss.
    unsigned int hitIndex = UINT_MAX;
    Vertex hitVertex;
    float hitDistSq = 0.f;

    searchKDTreeHit(kdTrees, ray, scene, hitIndex, hitVertex, hitDistSq);

    if (hitIndex == UINT_MAX) {
        statsMisses++;
        return BLACK;
    }
    statsHits++;
    float d = std::sqrt(hitDistSq);
    if (statsHits == 1 || d < statsMinHitDist) statsMinHitDist = d;
    if (d > statsMaxHitDist) statsMaxHitDist = d;

    // Debug visualizations: bypass the (currently stubbed) shading path and
    // return color that encodes the hit so we can verify each pipeline stage.
    switch (debugMode) {
        case DebugMode::HIT_MASK:
            return Vec3Df(255.f, 255.f, 255.f);
        case DebugMode::NORMALS: {
            Vec3Df n = hitVertex.getNormal();
            n.normalize();
            return Vec3Df(0.5f * (n[0] + 1.f) * 255.f,
                          0.5f * (n[1] + 1.f) * 255.f,
                          0.5f * (n[2] + 1.f) * 255.f);
        }
        case DebugMode::DEPTH: {
            float d = std::sqrt(hitDistSq);
            float t = depthRange > 0.f ? std::min(d / depthRange, 1.f) : 0.f;
            // Blue (near) → red (far). Easier to read than greyscale.
            return Vec3Df(t * 255.f, 0.f, (1.f - t) * 255.f);
        }
        case DebugMode::OBJECT_ID: {
            // Cheap hash so indices 0..N map to distinct-ish colors.
            float r = std::fmod(hitIndex * 157.f, 256.f);
            float g = std::fmod(hitIndex *  97.f + 64.f, 256.f);
            float b = std::fmod(hitIndex *  43.f + 128.f, 256.f);
            return Vec3Df(r, g, b);
        }
        case DebugMode::AMBIENT:
            return 255.f * scene->getObjects().at(hitIndex).getMaterial().getColor();
        case DebugMode::LIT:
        default:
            break;
    }

    couleurAmbient = scene->getObjects().at(hitIndex).getMaterial().getColor();

//    for(int i=0; i<scene->getLights().size(); i++){
//        int coeffShadow = 0;
//        float coeffDiffus = scene->getObjects().at(hitIndex).getMaterial().getDiffuse(),
//              coeffSpecular = scene->getObjects().at(hitIndex).getMaterial().getSpecular(),
//              intensityLight = scene->getLights().at(i).getIntensity();
//        Vec3Df lightColor = scene->getLights().at(i).getColor();

//        rayDiffus = Ray(hit.getPos() + EPS, scene->getLights().at(i).getPos() - hit.getPos());

//        if(softShadow){
//            const float & radius = scene->getLights()[i].getRadius();
//            const Vec3Df & normal = scene->getLights()[i].getPos() - intersectionPoint.getPos();

//            for(unsigned int j=0; j<rayPerLight; j++){
//                //On choisit un rayon au hasard en direction du disque
//                float randomRadius = qrand()  / (float) RAND_MAX * radius;

//                //On prend un vecteur au hasard->on en fait le produit vectoriel avec la normal->on normalise->on mutltiplie par le rayon
//                Vec3Df randomVec = Vec3Df::crossProduct(Vec3Df(qrand(),qrand(),qrand()),normal);
//                randomVec.normalize();
//                randomVec = intersectionPoint.getPos() + normal + randomVec * randomRadius;

//                //On crée le rayon associé
//                Ray softShadowRay = Ray(intersectionPoint.getPos() + EPS, randomVec);

//                if(ParcoursSceneExistence_v(arbresScene,softShadowRay,scene)) coeffShadow++ ;
//            }

//        }
//        else
//            if(ParcoursSceneExistence_v(arbresScene, rayDiffus, scene)) {coeffShadow = rayPerLight;}



//        if(coeffShadow < rayPerLight){
//            if(coeffDiffus*intensityLight*Vec3Df::dotProduct(rayDiffus.getDirection(), intersectionPoint.getNormal()) > 0){
//                Vec3Df temp = rayDiffus.getDirection();
//                temp.normalize();
//                couleurDiffus = couleurDiffus + ((rayPerLight - coeffShadow)/(float) rayPerLight)*coeffDiffus*intensityLight*Vec3Df::dotProduct(temp, intersectionPoint.getNormal())*lightColor ;
//            }
//        }

//        Vec3Df r = 2*(Vec3Df::dotProduct(-rayDiffus.getDirection(),intersectionPoint.getNormal()))*intersectionPoint.getNormal() + rayDiffus.getDirection();
//        r.normalize();
//        Vec3Df cameraPos = rayEmis.getOrigin();
//        cameraPos.normalize();
//        couleurSpecular = coeffSpecular*intensityLight*std::pow(std::max(Vec3Df::dotProduct(r,cameraPos),0.0f),10.0f)*lightColor;
//    }
//    couleurDiffus /= nb_lights;

    // if(this->ambientOcclusion)
   //    couleur *= (1.0 - intersectionPoint.getAmbientOcclusionCoeff());


    return 255.f * (0.33 * couleurDiffus + 0.33 * couleurAmbient + 0.33 * couleurSpecular);
};
