#include "KdTree2.h"

KdTree2::KdTree2(const vector<Vertex> & vertices, const vector<Triangle> & triangles, int _profondeur, int _precision, const Mesh & mesh){

	    //Racine
	    profondeur = _profondeur;
	    triangles_n = triangles;
	    vertices_n = vertices;
	    bbox_n = BoundingBox::computeBoundingBox(vertices);

	    Tg = NULL;
	    Td = NULL;

//	    cout<<"CREATION  : profondeur = "<<_profondeur <<" triangles.size = "<< triangles_n.size() ;
//	    cout<< " vertex.size = "<<vertices_n.size()<<" bbox.size = "<<bbox_n.getLength()<<" "<<bbox_n.getHeight()<<endl;
	    //Construction des sous arbres
	    //condition d'arret
	    if((triangles_n.size() > _precision) && (_profondeur <= 3)){
		isFeuille = false;
		//determine le vertex median selon l'axe
		int axe = determine_axe(bbox_n);
		axe_n = axe;
		median_n = determine_median(vertices_n, axe);
//		cout<<"l'axe median =  "<<axe<<endl;

		//mise à jour des vertex
		vector<Vertex> vertices_n_trie = sort_a(axe);
		vector<Vertex> vertices_n_g;
		vector<Vertex> vertices_n_d;
		decoupe(vertices_n_trie, vertices_n_g, vertices_n_d);
//		cout<<"nb vertex decoupés ="<<vertices_n_trie.size();
//		cout<<" à gauche = "<<vertices_n_g.size()<<" à droite = "<<vertices_n_d.size()<<endl;

		//mise à jour des triangles
		//on ne garde que les triangles qui ont un sommet dans le voxel
		vector<Triangle> triangles_n_g;
		vector<Triangle> triangles_n_d;
		partage_triangles(median_n,triangles_n, mesh, axe,
			       triangles_n_g,
			       triangles_n_d);
//		cout<<"nb triangles découpés = "<<triangles_n.size();
//		cout<<" à gauche = "<<triangles_n_g.size()<<" à droite = "<<triangles_n_d.size()<<endl;

		//création des sous arbres
		Tg = new KdTree2(vertices_n_g, triangles_n_g, profondeur + 1, _precision, mesh);
		Td = new KdTree2(vertices_n_d, triangles_n_d, profondeur + 1, _precision, mesh);

	    }
	    else{
		isFeuille = true;
	    }

};

bool tri_x_2(Vertex v1, Vertex v2){
    return v1.getPos()[0] < v2.getPos()[0] ;
};

bool tri_y_2(Vertex v1, Vertex v2){
    return v1.getPos()[1] < v2.getPos()[1] ;
};

bool tri_z_2(Vertex v1, Vertex v2){
    return v1.getPos()[2] < v2.getPos()[2] ;
};

inline vector<Vertex> KdTree2::sort_a(int axe){
    vector<Vertex> vertices_n_fils = vertices_n;
    if(axe == 0)
	sort (vertices_n_fils.begin(), vertices_n_fils.end(), tri_x_2);
    if(axe == 1)
	sort (vertices_n_fils.begin(), vertices_n_fils.end(), tri_y_2);
    if(axe == 2)
	sort (vertices_n_fils.begin(), vertices_n_fils.end(), tri_z_2);

    return vertices_n_fils ;


};

inline Vertex KdTree2::milieu(){
    int b = vertices_n.size()/2;
    return vertices_n[b];
};

inline void KdTree2::decoupe(const vector<Vertex> & n, vector<Vertex>  & n_g, vector<Vertex> & n_d){
    int median = n.size()/2;
    if(n.size()%2 != 0){
	median += 1 ;//verifier que l'on en oublie pas un
    }

    for(int i=0; i<median;i++){
	n_g.push_back(n[i]);
	n_d.push_back(n[n.size() - i]);
    }
//    cout<<"median ="<<median<<" "<<n_g.size()<<endl;
};

inline int KdTree2::determine_axe(const BoundingBox & bbox){
    float max = 0;
    int axe =0;
    for(int i=0;i<3;i++){
	if(bbox.getWHL(i)>max){
	    axe = i;
	    max = bbox.getWHL(i);
	}
    }
    return axe;
};

//Determine le vertex median pour un axe donné
inline Vertex KdTree2::determine_median(vector<Vertex> n, int axe){
    vector<Vertex> vertices_n_fils = n;
    if(axe == 0)
	sort (vertices_n_fils.begin(), vertices_n_fils.end(), tri_x_2);
    if(axe == 1)
	sort (vertices_n_fils.begin(), vertices_n_fils.end(), tri_y_2);
    if(axe == 2)
	sort (vertices_n_fils.begin(), vertices_n_fils.end(), tri_z_2);

    int milieu = vertices_n_fils.size()/2;
    return vertices_n_fils[milieu] ;
};

//Partage les triangles en fonction du vertex median trouvé précédemment
inline void KdTree2::partage_triangles(Vertex & median, vector<Triangle> & triangles_n, const Mesh & mesh, int axe,
		       vector<Triangle> & triangles_n_g,
		       vector<Triangle> & triangles_n_d){

    vector<Vertex> m = mesh.getVertices();
    for(unsigned int i=0;i<triangles_n.size();i++){
	bool appartient_a_g = false;
	bool appartient_a_d = false;

	for(int k =0;k<3;k++){
	    //si un des vertex du triangle est dans le voxel on le garde
	    if(m[triangles_n[i].getVertex(k)].getPos()[axe] < median.getPos()[axe]){
		if(!appartient_a_g){
		    triangles_n_g.push_back(triangles_n[i]);
		    appartient_a_g = true;
		}
	    }
	    else{
		if(!appartient_a_d){
		    triangles_n_d.push_back(triangles_n[i]);
		    appartient_a_d = true;
		}
	    }

	}

    } //fin du parcours des triangles
};

//Triangle KdTree::intersect(Ray r, float t_min, float t_max){};
//Triangle KdTree::traverse(Ray r, float t_min, float t_max, KdTree * n){};

bool KdTree2::recParcoursArbreExistence_v(Ray & r, const Mesh & mesh){

    Vec3Df neSertaRien;

    //Cas triviaux
    if(this==NULL){
	return false;
    }
    if(!r.intersect(this->bbox_n,neSertaRien)){
	return false;
    }
    if(this->isFeuille){
	//donne juste l'existence (plus rapide)
	////!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	//return r.existeIntersectionVecteur(object, this->triangles_n);

    }

    else{
	//on test l'intersection avec la boite englobante des sous arbres
	Vec3Df t_split_min, t_split_max;
	bool intersect_Tg = r.intersect(this->bbox_n, t_split_min);
	bool intersect_Td = r.intersect(this->bbox_n, t_split_max);

	if(intersect_Tg && !intersect_Td){
	    return this->Tg->recParcoursArbreExistence_v( r, mesh);
	}
	else if(!intersect_Tg && intersect_Td){
	    return this->Td->recParcoursArbreExistence_v( r, mesh);
	}
	else{

		//seul le premier est calculé si il trouve une intersection
		return this->Td->recParcoursArbreExistence_v( r, mesh) ||
			this->Tg->recParcoursArbreExistence_v( r, mesh) ;
	    }
	}



};


bool KdTree2::recParcoursArbre_v(Ray & r, const Mesh & mesh, Vertex & intersectionPoint, float & distance){
    bool resultat_parcours_gauche;
    bool resultat_parcours_droit;
    Vec3Df neSertaRien;

    //Cas triviaux
    if(this==NULL){
	return false;
    }
    if(!r.intersect(this->bbox_n,neSertaRien)){
	return false;
    }
    if(this->isFeuille){
	//retourne le point d'intersection le plus proche pour le vecteur de triangles !
	//met à jour la valeur de distance;
	//!!!!!!!!!!!!!!!!!!!!!
	//return r.intersectVecteurDeTriangles_v(object, this->triangles_n, intersectionPoint, distance);

    }



    else{
	//on test l'intersection avec la boite englobante des sous arbres
	Vec3Df t_split_min, t_split_max;
	bool intersect_Tg = r.intersect(this->bbox_n, t_split_min);
	bool intersect_Td = r.intersect(this->bbox_n, t_split_max);

	if(intersect_Tg && !intersect_Td){
	    return this->Tg->recParcoursArbre_v( r, mesh, intersectionPoint, distance);
	}
	else if(!intersect_Tg && intersect_Td){
	    return this->Td->recParcoursArbre_v( r, mesh, intersectionPoint, distance);
	}
	else{
	//le rayon intersecte les deux bbox_n
	//il faut etre plus précis
	//DEMANDER QUE RETOUNRE Ray::intersect(boundinBox ...

/*	    //on teste seulement le sous arbre le plus proche
	    if(Vec3Df::squaredDistance(r.getOrigin(), t_split_min)< Vec3Df::squaredDistance(r.getOrigin(), t_split_max)){

		//on teste juste l'existence
		if(r.intersect(object, n->Tg->triangles_n)){
		    return recParcoursArbre(n->Tg, r, object, intersectionPoint, distance);
		}
	    }
	    else{
		//on teste juste l'existence
		if(r.intersect(object, n->Td->triangles_n)){
		    return recParcoursArbre(n->Td, r, object, intersectionPoint, distance);
		}
		}
*/

		resultat_parcours_droit = this->Td->recParcoursArbre_v( r, mesh, intersectionPoint, distance);
		resultat_parcours_gauche = this->Tg->recParcoursArbre_v( r, mesh, intersectionPoint, distance);

		return resultat_parcours_droit || resultat_parcours_gauche ;
	    }
	}



};

int KdTree2::getProfondeurArbre(){
    if(this == NULL){
	return 0;
    }
    return (1 + this->Td->getProfondeurArbre() );
};

void KdTree2::recDrawBoundingBox(int _profondeur){

    if(this->profondeur == _profondeur){
	this->bbox_n.drawBoundingBox();
    }
    else{
	this->Td->recDrawBoundingBox(_profondeur);
	this->Tg->recDrawBoundingBox(_profondeur);
    }
};


