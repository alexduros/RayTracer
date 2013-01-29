#ifndef KDTREE_H
#define KDTREE_H
////////////////////////////////////////////////////////
//
//          Un KdTree par object de la scene
//
//
////////////////////////////////////////////////////////
#include <vector>
#include "Ray.h"
//#include "Object.h"


using namespace std;

class KdTree{

public :
        int numero;
unsigned int profondeur;
bool isFeuille;
int axe_n;
Vertex median_n;
BoundingBox bbox_n;
vector<Triangle> triangles_n;
vector<Vertex> vertices_n;
KdTree* Tg;
KdTree* Td;

inline vector<Vertex> sort_a(int axe);
static inline int determine_axe(const BoundingBox & bbox);
inline Vertex milieu();
static inline void decoupe(const vector<Vertex> & n, vector<Vertex> & n_g, vector<Vertex> & n_d);
static inline Vertex determine_median(vector<Vertex> n, int axe);
static inline void partage_triangles(Vertex & median, vector<Triangle> & triangles_n, const Mesh & mesh, int axe,
                                     vector<Triangle> & triangles_n_g,
                                     vector<Triangle> & triangles_n_d);

//Constructeur par défaut
KdTree() : numero(0), profondeur(0), Tg(NULL), Td(NULL) {}
//Constructeur recursif
KdTree(const vector<Vertex> & vertices, const vector<Triangle> & triangles,  unsigned int _profondeur, int precision, const Mesh & mesh);

bool recParcoursArbreExistence_v(Ray & r, const Mesh & mesh);
bool recParcoursArbre_v(Ray & r, const Mesh & mesh, Vertex & intersectionPoint, float & distance);

int getProfondeurArbre();
void recDrawBoundingBox(unsigned int profondeur);

inline void renderGL (unsigned int depth ){

    if (depth <= this->profondeur) {
        bbox_n.renderGL();

    }

    else if(this->Tg != NULL && this->Td != NULL){

        this->Tg->renderGL(depth);
        this->Td->renderGL(depth);
    }
    else if(this->Tg == NULL && this->Td == NULL){
        bbox_n.renderGL();
    }
}


};


#endif // KDTREE_H
