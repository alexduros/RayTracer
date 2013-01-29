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

const Vec3Df EPS = Vec3Df(0.001,0.001,0.001);
static RayTracer * instance = NULL;

RayTracer * RayTracer::getInstance () {
    if (instance == NULL)
        instance = new RayTracer ();
    return instance;
}

void RayTracer::destroyInstance () {
    if (instance != NULL) {
        delete instance;
        instance = NULL;
    }
}

inline int clamp (float f, int inf, int sup) {
    int v = static_cast<int> (f);
    return (v < inf ? inf : (v > sup ? sup : v));
}

// POINT D'ENTREE DU PROJET.
// Le code suivant ray trace uniquement la boite englobante de la scene.
// Il faut remplacer ce code par une veritable raytracer
QImage RayTracer::render (const Vec3Df & camPos,
                          const Vec3Df & direction,
                          const Vec3Df & upVector,
                          const Vec3Df & rightVector,
                          float fieldOfView,
                          float aspectRatio,
                          unsigned int screenWidth,
                          unsigned int screenHeight) {
    QImage image (QSize (screenWidth, screenHeight), QImage::Format_RGB888);

    clock_t start, finish;
    //cout << "Rendering started..." << endl;
    start = clock();

    Scene * scene = Scene::getInstance ();
    int isCalculated = 0;
    if(this->ambientOcclusion && isCalculated==0){
        scene->calculAmbientOcclusion();
        isCalculated++;
    }

    const BoundingBox & bbox = scene->getBoundingBox ();
    const Vec3Df & minBb = bbox.getMin ();
    const Vec3Df & maxBb = bbox.getMax ();



    //on creer un arbre par object de la scene
    vector<KdTree *> arbresScene;
    creerArbres(scene, arbresScene);
    arbresScene[1]->recDrawBoundingBox(2);

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
            Vec3Df intersectionPoint,c;

            if(this->antialiasing){
                const int & dimRay = this->numDir;
                if(this->mode == 0){
                    // Anti-aliasing "régulier"
                    float coeff =1;
                    for (unsigned int a = 0; a<dimRay; a++)
                        for(unsigned int b = 0; b<dimRay; b++) {
                        Vec3Df stepX = (float (i) - screenWidth/2.f + a*coeff)/screenWidth * tanX * rightVector;
                        Vec3Df stepY = (float (j) - screenHeight/2.f + b*coeff)/screenHeight * tanY * upVector;
                        Vec3Df dir = direction + stepX + stepY;
                        dir.normalize();
                        Ray ray(camPos, dir);
                        c += RayTracer::lancerDeRayon(arbresScene, scene, ray);
                        c /=(dimRay*dimRay);
                    }
                }
                if(this->mode == 1){
                    Vec3Df c;
                    // Anti-aliasing stochastique:
                    for (unsigned int a = 0; a<dimRay; a++) {
                        Vec3Df stepX = (float (i) - screenWidth/2.f + rand()/(float(RAND_MAX)+1))/screenWidth * tanX * rightVector;
                        Vec3Df stepY = (float (j) - screenHeight/2.f + rand()/(float(RAND_MAX)+1))/screenHeight * tanY * upVector;
                        Vec3Df dir = direction + stepX + stepY;
                        dir.normalize();
                        Ray ray(camPos, dir);
                        c += RayTracer::lancerDeRayon(arbresScene, scene, ray);
                    }
                    c /= dimRay;
                }
            }
            else
                c=RayTracer::lancerDeRayon(arbresScene, scene, ray);

            image.setPixel (i, ((screenHeight-1)-j), qRgb (clamp (c[0], 0, 255),
                                                           clamp (c[1], 0, 255),
                                                           clamp (c[2], 0, 255)));

        }
    }

    finish = clock();
    cout << "Rendering finished after " << ((float)(finish - start) / CLOCKS_PER_SEC) << " seconds" << endl;
    return image;
}


////////////////////////////////////////////////////////////////
//
// Gestion des KdTree dans le lancer de rayon
//
////////////////////////////////////////////////////////////////

/////////////////////////
//Parcours dans un arbre
/////////////////////////
bool RayTracer::recParcoursArbre_v(KdTree * n, Ray & r, const Object & object, Vertex & intersectionPoint, float & distance){
    bool resultat_parcours_gauche;
    bool resultat_parcours_droit;
    Vec3Df neSertaRien;

    //Cas triviaux
    if(n==NULL){
        return false;
    }
    if(!r.intersect(n->bbox_n,neSertaRien)){
        return false;
    }
    if(n->isFeuille){
        //retourne le point d'intersection le plus proche pour le vecteur de triangles !
        //met à jour la valeur de distance;
        return r.intersectVecteurDeTriangles_v(object.getMesh(), n->triangles_n, intersectionPoint, distance);

    }



    else{
        //on test l'intersection avec la boite englobante des sous arbres
        Vec3Df t_split_min, t_split_max;
        bool intersect_Tg = r.intersect(n->bbox_n, t_split_min);
        bool intersect_Td = r.intersect(n->bbox_n, t_split_max);

        if(intersect_Tg && !intersect_Td){
            return recParcoursArbre_v(n->Tg, r, object, intersectionPoint, distance);
        }
        else if(!intersect_Tg && intersect_Td){
            return recParcoursArbre_v(n->Td, r, object, intersectionPoint, distance);
        }
        else{
            resultat_parcours_droit = recParcoursArbre_v(n->Td, r, object, intersectionPoint, distance);
            resultat_parcours_gauche = recParcoursArbre_v(n->Tg, r, object, intersectionPoint, distance);

            return resultat_parcours_droit || resultat_parcours_gauche ;
        }
    }

};

////////////////////////////////////////
// Donne juste l'existence (plus rapide)
////////////////////////////////////////
bool RayTracer::recParcoursArbreExistence_v(KdTree * n, Ray & r, const Object & object){

    Vec3Df neSertaRien;

    //Cas triviaux
    if(n==NULL){
        return false;
    }
    if(!r.intersect(n->bbox_n,neSertaRien)){
        return false;
    }
    if(n->isFeuille){
        //donne juste l'existence (plus rapide)
        return r.existeIntersectionVecteur(object.getMesh(), n->triangles_n);

    }

    else{
        //on test l'intersection avec la boite englobante des sous arbres
        Vec3Df t_split_min, t_split_max;
        bool intersect_Tg = r.intersect(n->bbox_n, t_split_min);
        bool intersect_Td = r.intersect(n->bbox_n, t_split_max);

        if(intersect_Tg && !intersect_Td){
            return recParcoursArbreExistence_v(n->Tg, r, object);
        }
        else if(!intersect_Tg && intersect_Td){
            return recParcoursArbreExistence_v(n->Td, r, object);
        }
        else{
            //seul le premier est calculé si il trouve une intersection
            return recParcoursArbreExistence_v(n->Td, r, object) ||
                    recParcoursArbreExistence_v(n->Tg, r, object) ;
        }
    }
};

////////////////////////////////
//Parcours la Scene
//(tous les arbres)
////////////////////////////////
bool RayTracer::ParcoursScene_v(vector<KdTree *> arbresScenes, Ray & r, Scene * s, Vertex & intersectionPoint, int & numObject){
    bool resultat = false;
    float distance = 1000;
    int nbObjects = arbresScenes.size();
    float distanceTemp = 1000;


    for(int i=0;i<nbObjects;i++){

        resultat = RayTracer::recParcoursArbre_v(arbresScenes[i],r,s->getObjects()[i],intersectionPoint, distance) || resultat;

        if( distance < distanceTemp ){
            numObject = i;
            distanceTemp = distance;
        }

    }
    return resultat;
};

/////////////////////////////////////////
//Parcours dans la scene
//dans tous les arbres (rapide)
/////////////////////////////////////////
bool RayTracer::ParcoursSceneExistence_v(vector<KdTree *> arbresScenes, Ray & r, Scene * s){
    bool resultat = false;
    int nbObjects = arbresScenes.size();

    for(int i=0;i<nbObjects;i++){
        resultat = resultat || RayTracer::recParcoursArbreExistence_v(arbresScenes[i],r,s->getObjects()[i]) ;
        //on arrete la boucle dès qu'on a trouvé une intersection
    }
    return resultat;
};

//////////////////////////////
//on creer un arbre par objet
//////////////////////////////
void RayTracer::creerArbres(Scene * scene, vector<KdTree *> & arbresScene){

    int nb_object = scene->getObjects().size();

    for(int i=0;i<nb_object;i++){
        vector<Triangle> T = scene->getObjects()[i].getMesh().getTriangles();
        vector<Vertex> V = scene->getObjects()[i].getMesh().getVertices();

        KdTree * n = new KdTree(V,T, 0, 300,scene->getObjects()[i].getMesh());
        arbresScene.push_back(n);
    }
};

///////////////////////////////////////////////////////////////////////////////////////////
//
// Lancer de Rayon
// - retourne un Vec3Df qui contient la couleur du pixel
//
///////////////////////////////////////////////////////////////////////////////////////////

Vec3Df RayTracer::lancerDeRayon(vector<KdTree*> & arbresScene, Scene * scene, Ray & rayEmis ){
    Vec3Df couleur = Vec3Df(0.f,0.f,0.f);
    Vec3Df couleurTransmis, couleurReflechis, couleurDiffus, couleurAmbient, couleurSpecular;
    Ray rayTransmis, rayReflechis, rayDiffus;
    int numObject = 1;
    int nb_lights = scene->getLights().size();

    Vertex intersectionPoint = Vertex(Vec3Df(1000.0,1000.0,1000.0));

    if(!ParcoursScene_v(arbresScene, rayEmis, scene, intersectionPoint, numObject)){
        return Vec3Df(0.0,0.0,0.0);
    }
    else{
        couleurAmbient = scene->getObjects()[numObject].getMaterial().getColor();


        const int & rayPerLight = this->rayPerLight;

        couleurDiffus =Vec3Df(0.0,0.0,0.0);
        couleurSpecular =Vec3Df(0.0,0.0,0.0);


        bool softShadow = this->shadowMode;
        for(int i = 0;i<nb_lights; i++){
            int coeffShadow = 0;
            float coeffDiffus = scene->getObjects()[numObject].getMaterial().getDiffuse();
            float coeffSpecular = scene->getObjects()[numObject].getMaterial().getSpecular();
            float intensityLight = scene->getLights()[i].getIntensity();
            Vec3Df lightColor = scene->getLights()[i].getColor();

            rayDiffus = Ray(intersectionPoint.getPos() + EPS,scene->getLights()[i].getPos() - intersectionPoint.getPos());

            if(softShadow){
                const float & radius = scene->getLights()[i].getRadius();
                const Vec3Df & normal = scene->getLights()[i].getPos() - intersectionPoint.getPos();

                for(unsigned int j=0; j<rayPerLight; j++){
                    //On choisit un rayon au hasard en direction du disque
                    float randomRadius = qrand()  / (float) RAND_MAX * radius;

                    //On prend un vecteur au hasard->on en fait le produit vectoriel avec la normal->on normalise->on mutltiplie par le rayon
                    Vec3Df randomVec = Vec3Df::crossProduct(Vec3Df(qrand(),qrand(),qrand()),normal);
                    randomVec.normalize();
                    randomVec = intersectionPoint.getPos() + normal + randomVec * randomRadius;

                    //On crée le rayon associé
                    Ray softShadowRay = Ray(intersectionPoint.getPos() + EPS, randomVec);

                    if(ParcoursSceneExistence_v(arbresScene,softShadowRay,scene)) coeffShadow++ ;
                }

            }
            else
                if(ParcoursSceneExistence_v(arbresScene, rayDiffus, scene)) {coeffShadow = rayPerLight;}



            if(coeffShadow < rayPerLight){
                if(coeffDiffus*intensityLight*Vec3Df::dotProduct(rayDiffus.getDirection(), intersectionPoint.getNormal()) > 0){
                    Vec3Df temp = rayDiffus.getDirection();
                    temp.normalize();
                    couleurDiffus = couleurDiffus + ((rayPerLight - coeffShadow)/(float) rayPerLight)*coeffDiffus*intensityLight*Vec3Df::dotProduct(temp, intersectionPoint.getNormal())*lightColor ;
                }
            }

            Vec3Df r = 2*(Vec3Df::dotProduct(-rayDiffus.getDirection(),intersectionPoint.getNormal()))*intersectionPoint.getNormal() + rayDiffus.getDirection();
            r.normalize();
            Vec3Df cameraPos = rayEmis.getOrigin();
            cameraPos.normalize();
            couleurSpecular = coeffSpecular*intensityLight*std::pow(std::max(Vec3Df::dotProduct(r,cameraPos),0.0f),10.0f)*lightColor;
        }
        couleurDiffus /= nb_lights;
    }

    couleur = 0.33*couleurDiffus + 0.33*couleurAmbient + 0.33*couleurSpecular;
    if(this->ambientOcclusion)
        couleur *= (1.0 - intersectionPoint.getAmbientOcclusionCoeff());

    couleur*=255.f;
    return couleur;
};
