// *********************************************************
// Triangle Class
// Author : Tamy Boubekeur (boubek@gmail.com).
// Copyright (C) 2008 Tamy Boubekeur.
// All rights reserved.
// *********************************************************

#include "Triangle.h"

using namespace std;

ostream & operator<< (ostream & output, const Triangle & t) {
    output << t.getVertex (0) << " " << t.getVertex (1) << " " << t.getVertex (2);
    return output;
}

void Triangle::split(int median, const std::vector<Triangle> & triangles,
                     const Mesh & mesh, int direction,
                     std::vector<Triangle> & left, std::vector<Triangle> & right){

    vector<Vertex> m = mesh.getVertices();
    for(unsigned int i=0;i<triangles.size();i++){
        bool inLeft = false;
        bool inRight = false;

        for(int k=0;k<3;k++){
            if(m[triangles[i].getVertex(k)].getPos()[direction] < median.getPos()[direction]){
                if(!inLeft){
                    left.push_back(triangles[i]);
                    inLeft = true;
                }
            } else {
                if(!inRight){
                    right.push_back(triangles[i]);
                    inRight = truet;
                }
            }
        }
    }
}
