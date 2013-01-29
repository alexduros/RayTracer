#ifndef KDTREE2_H
#define KDTREE2_H

////////////////////////////////////////////////////////
//
//          Un KdTree par object de la scene
//
//
////////////////////////////////////////////////////////
#include <vector>
#include "Ray.h"
#include "Mesh.h"


using namespace std;

class KdTree2{

    public :
	int numero;
	int profondeur;
	bool isFeuille;
	int axe_n;
	Vertex median_n;
	BoundingBox bbox_n;
	vector<Triangle> triangles_n;
	vector<Vertex> vertices_n;
	KdTree2* Tg;
	KdTree2* Td;

	inline vector<Vertex> sort_a(int axe);
	static inline int determine_axe(const BoundingBox & bbox);
	inline Vertex milieu();
	static inline void decoupe(const vector<Vertex> & n, vector<Vertex> & n_g, vector<Vertex> & n_d);
	static inline Vertex determine_median(vector<Vertex> n, int axe);
	static inline void partage_triangles(Vertex & median, vector<Triangle> & triangles_n, const Mesh & mesh, int axe,
			       vector<Triangle> & triangles_n_g,
			       vector<Triangle> & triangles_n_d);

	//Constructeur par défaut
	KdTree2() : numero(0), profondeur(0), Tg(NULL), Td(NULL) {};
	//Constructeur recursif
	KdTree2(const vector<Vertex> & vertices, const vector<Triangle> & triangles,  int _profondeur, int precision, const Mesh & mesh);

	//Triangle intersect(Ray r, float t_min, float t_max);
	//Triangle traverse(Ray r, float t_min, float t_max, KdTree * n);
	//bool recParcours(KdTree * n, Ray & r, Vec3Df intersectionPoint);
	bool recParcoursArbreExistence_v(Ray & r, const Mesh & mesh);
	bool recParcoursArbre_v(Ray & r, const Mesh & mesh, Vertex & intersectionPoint, float & distance);

	int getProfondeurArbre();
	void recDrawBoundingBox(int profondeur);
};

#endif // KDTREE2_H
