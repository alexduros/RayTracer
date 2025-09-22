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
    return image;
}

void RayTracer::searchKDTreeHit(std::vector<KdTree *> kdTrees, const Ray & ray, Scene * scene, unsigned int & kdTree){
    float distance = 0.f;
    for(unsigned int i=0; i<scene->getObjects().size(); i++ ) {
        Vertex hit;
        if(kdTrees.at(i)->searchHit(ray, hit, distance)){
            kdTree = i;
        };
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
    unsigned int hitIndex;

    searchKDTreeHit(kdTrees, ray, scene, hitIndex);

    if(!hitIndex){
        return BLACK;
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
