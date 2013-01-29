#ifndef TRIANGLEC_H
#define TRIANGLEC_H

#include "Vertex.h"
#include <vector>

class TriangleC {
private :
        Vertex v[3];
    public :

            TriangleC(Vertex v0, Vertex v1, Vertex v2){
        v[0]=v0;
        v[1]=v1;
        v[2]=v2;
    }
    inline Vertex getVertex (unsigned int i) const { return v[i]; }
    inline void setVertex (unsigned int i, const Vertex & vertex) { v[i] = vertex; }
    //inline bool contains (const Vertex & vertex) const { return (v[0] == vertex || v[1] == vertex || v[2] == vertex); }
};
#endif // TRIANGLEC_H
