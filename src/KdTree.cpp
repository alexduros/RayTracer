#include "KdTree.h"

///////////////
//Constructeur
///////////////
KdTree::KdTree(const vector<Vertex> & vertices, const vector<Triangle> & triangles, unsigned int _profondeur, int _precision, const Mesh & mesh){

    //Racine
    this->profondeur = _profondeur;
    this->triangles_n = triangles;
    this->vertices_n = vertices;
    this->bbox_n = BoundingBox::computeBoundingBoxTriangles( triangles, mesh);

    this->Tg = NULL;
    this->Td = NULL;



    //Construction des sous arbres
    //condition d'arret
    if((triangles_n.size() > _precision) && (_profondeur <= 15)){
        isFeuille = false;
        //determine le vertex median selon l'axe
        int axe = determine_axe(bbox_n);
        axe_n = axe;
        median_n = determine_median(vertices_n, axe);

        //mise à jour des vertex
        vector<Vertex> vertices_n_trie = sort_a(axe);
        vector<Vertex> vertices_n_g;
        vector<Vertex> vertices_n_d;
        decoupe(vertices_n_trie, vertices_n_g, vertices_n_d);

        //mise à jour des triangles
        //on ne garde que les triangles qui ont un sommet dans le voxel
        vector<Triangle> triangles_n_g;
        vector<Triangle> triangles_n_d;
        partage_triangles(median_n,triangles_n, mesh, axe,
                          triangles_n_g,
                          triangles_n_d);

        //création des sous arbres
        if(vertices_n_g.size() !=0 && triangles_n_g.size() != 0){
            Tg = new KdTree(vertices_n_g, triangles_n_g, profondeur + 1, _precision, mesh);
        }
        if(vertices_n_d.size() !=0 && triangles_n_d.size() != 0){
            Td = new KdTree(vertices_n_d, triangles_n_d, profondeur + 1, _precision, mesh);
        }

    }
    else{
        isFeuille = true;
    }

};

/////////////////
//Les 3 tris possibles
//////////////////////
//width
bool tri_x(Vertex v1, Vertex v2){
    return v1.getPos()[0] < v2.getPos()[0] ;
};
//height
bool tri_y(Vertex v1, Vertex v2){
    return v1.getPos()[1] < v2.getPos()[1] ;
};
//length
bool tri_z(Vertex v1, Vertex v2){
    return v1.getPos()[2] < v2.getPos()[2] ;
};

///////////////
//tri selon l'axe
//////////////////
inline vector<Vertex> KdTree::sort_a(int axe){
    vector<Vertex> vertices_n_fils = vertices_n;
    if(axe == 0)
        sort (vertices_n_fils.begin(), vertices_n_fils.end(), tri_x);
    if(axe == 1)
        sort (vertices_n_fils.begin(), vertices_n_fils.end(), tri_y);
    if(axe == 2)
        sort (vertices_n_fils.begin(), vertices_n_fils.end(), tri_z);

    return vertices_n_fils ;


};

///////////
//milieu
//////////
inline Vertex KdTree::milieu(){
    int b = vertices_n.size()/2;
    return vertices_n[b];
};

///////////
//découpe
///////////
inline void KdTree::decoupe(const vector<Vertex> & n, vector<Vertex>  & n_g, vector<Vertex> & n_d){
    if(n.size() != 0){
        int median = n.size()/2;
        if(n.size()%2 != 0){
            median += 1 ;
        }

        for(int i=0; i<median; i++)n_g.push_back(n[i]);
        for(int i=median; i<n.size();i++)n_d.push_back(n[i]);

    }
};

inline int KdTree::determine_axe(const BoundingBox & bbox){
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

///////////////////////////////////////////////
//Determine le vertex median pour un axe donné
///////////////////////////////////////////////
inline Vertex KdTree::determine_median(vector<Vertex> n, int axe){
    vector<Vertex> vertices_n_fils = n;
    if(axe == 0)
        sort (vertices_n_fils.begin(), vertices_n_fils.end(), tri_x);
    if(axe == 1)
        sort (vertices_n_fils.begin(), vertices_n_fils.end(), tri_y);
    if(axe == 2)
        sort (vertices_n_fils.begin(), vertices_n_fils.end(), tri_z);

    int milieu = vertices_n_fils.size()/2;
    return vertices_n_fils[milieu] ;
};

////////////////////////////////////////////////////////////////////////
//Partage les triangles en fonction du vertex median trouvé précédemment
////////////////////////////////////////////////////////////////////////
inline void KdTree::partage_triangles(Vertex & median, vector<Triangle> & triangles_n, const Mesh & mesh, int axe,
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

////////////////
//Profondeur
//////////////////
int KdTree::getProfondeurArbre(){
    if(this == NULL){
        return 0;
    }
    return (1 + this->Td->getProfondeurArbre() );
};

/////////////
//DrawBoundingBox
///////////////
void KdTree::recDrawBoundingBox(unsigned int _profondeur){

    if(this->profondeur == _profondeur){
        this->bbox_n.renderGL();
    }
    else{
        this->Td->recDrawBoundingBox(_profondeur);
        this->Tg->recDrawBoundingBox(_profondeur);
    }
};



/////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
bool KdTree::recParcoursArbreExistence_v(Ray & r, const Mesh & mesh){

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
        return r.existeIntersectionVecteur(mesh, this->triangles_n);

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


bool KdTree::recParcoursArbre_v(Ray & r, const Mesh & mesh, Vertex & intersectionPoint, float & distance){
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
        return r.intersectVecteurDeTriangles_v(mesh, this->triangles_n, intersectionPoint, distance);

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
            resultat_parcours_droit = this->Td->recParcoursArbre_v( r, mesh, intersectionPoint, distance);
            resultat_parcours_gauche = this->Tg->recParcoursArbre_v( r, mesh, intersectionPoint, distance);

            return resultat_parcours_droit || resultat_parcours_gauche ;
        }
    }



};
