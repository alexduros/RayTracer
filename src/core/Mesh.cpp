// ---------------------------------------------------------
// Mesh Class
// Author : Tamy Boubekeur (boubek@gmail.com).
// Copyright (C) 2008 Tamy Boubekeur.
// All rights reserved.
// ---------------------------------------------------------

#include "Mesh.h"
#include <algorithm>
#include <fstream>
#include <stdexcept>

using namespace std;

void Mesh::clear () {
    clearTopology ();
    clearGeometry ();
}

void Mesh::clearGeometry () {
    vertices.clear ();
}

void Mesh::clearTopology () {
    triangles.clear ();
}

void Mesh::unmarkAllVertices () {
    for (Vertex & v : vertices)
        v.unmark ();
}

void Mesh::computeTriangleNormals (vector<Vec3Df> & triangleNormals) const {
    triangleNormals.clear ();
    triangleNormals.reserve (triangles.size ());
    for (const Triangle & t : triangles) {
        Vec3Df e01 (vertices[t.getVertex (1)].getPos () - vertices[t.getVertex (0)].getPos ());
        Vec3Df e02 (vertices[t.getVertex (2)].getPos () - vertices[t.getVertex (0)].getPos ());
        Vec3Df n (Vec3Df::crossProduct (e01, e02));
        n.normalize ();
        triangleNormals.push_back (n);
    }
}

void Mesh::recomputeSmoothVertexNormals (unsigned int normWeight) {
    vector<Vec3Df> triangleNormals;
    computeTriangleNormals (triangleNormals);
    for (Vertex & v : vertices)
        v.setNormal (Vec3Df (0.0, 0.0, 0.0));
    for (size_t i = 0; i < triangles.size (); i++) {
        const Triangle & t = triangles[i];
        const Vec3Df & tn = triangleNormals[i];
        for (unsigned int j = 0; j < 3; j++) {
            Vertex & vj = vertices[t.getVertex (j)];
            float w = 1.0; // uniform weights
            Vec3Df e0 = vertices[t.getVertex ((j+1)%3)].getPos () - vj.getPos ();
            Vec3Df e1 = vertices[t.getVertex ((j+2)%3)].getPos () - vj.getPos ();
            if (normWeight == 1) { // area weight
                w = Vec3Df::crossProduct (e0, e1).getLength () / 2.0;
            } else if (normWeight == 2) { // angle weight
                e0.normalize ();
                e1.normalize ();
                w = (2.0 - (Vec3Df::dotProduct (e0, e1) + 1.0)) / 2.0;
            }
            if (w <= 0.0)
                continue;
            vj.setNormal (vj.getNormal () + tn * w);
        }
    }
    Vertex::normalizeNormals (vertices);
}

void Mesh::collectOneRing (vector<vector<unsigned int> > & oneRing) const {
    oneRing.resize (vertices.size ());
    for (unsigned int i = 0; i < triangles.size (); i++) {
        const Triangle & ti = triangles[i];
        for (unsigned int j = 0; j < 3; j++) {
            unsigned int vj = ti.getVertex (j);
            for (unsigned int k = 1; k < 3; k++) {
                unsigned int vk = ti.getVertex ((j+k)%3);
                if (find (oneRing[vj].begin (), oneRing[vj].end (), vk) == oneRing[vj].end ())
                    oneRing[vj].push_back (vk);
            }
        }
    }
}

void Mesh::collectOrderedOneRing (vector<vector<unsigned int> > & oneRing) const {
    oneRing.resize (vertices.size ());
    for (unsigned int t = 0; t < triangles.size (); t++) {
        const Triangle & ti = triangles[t];
        for (unsigned int i = 0; i < 3; i++) {
            unsigned int vi = ti.getVertex (i);
            unsigned int vj = ti.getVertex ((i+1)%3);
            unsigned int vk = ti.getVertex ((i+2)%3);
            vector<unsigned int> & oneRingVi = oneRing[vi];
            vector<unsigned int>::iterator begin = oneRingVi.begin ();
            vector<unsigned int>::iterator end = oneRingVi.end ();
            vector<unsigned int>::iterator nj = find (begin, end, vj);
            vector<unsigned int>::iterator nk = find (begin, end, vk);
            if (nj != end && nk == end) {
                if (nj == begin)
                    nj = end;
                nj--;
                oneRingVi.insert (nj, vk);
            } else if (nj == end && nk != end)
                oneRingVi.insert (nk, vj);
            else if (nj == end && nk == end) {
                oneRingVi.push_back (vk);
                oneRingVi.push_back (vj);
            }
        }
    }
}

void Mesh::computeDualEdgeMap (EdgeMapIndex & dualVMap1, EdgeMapIndex & dualVMap2) {
    for (const Triangle & t : triangles) {
        for (unsigned int i = 0; i < 3; i++) {
            Edge eij (t.getVertex (i), t.getVertex ((i+1)%3));
            if (dualVMap1.find (eij) == dualVMap1.end ())
                dualVMap1[eij] = t.getVertex ((i+2)%3);
            else
                dualVMap2[eij] = t.getVertex ((i+2)%3);
        }
    }
}

void Mesh::markBorderEdges (EdgeMapIndex & edgeMap) {
    for (const Triangle & t : triangles) {
        for (unsigned int i = 0; i < 3; i++) {
            unsigned int j = (i+1)%3;
            Edge eij (t.getVertex (i), t.getVertex (j));
            if (edgeMap.find (eij) == edgeMap.end ())
                edgeMap[eij] = 0;
            else
                edgeMap[eij] += 1;
        }
    }
}

void Mesh::loadOFF (const std::string & filename) {
    clear ();
    ifstream input (filename.c_str ());
    if (!input)
        throw runtime_error ("Mesh::loadOFF: cannot open " + filename);
    string magic_word;
    input >> magic_word;
    if (magic_word != "OFF")
        throw runtime_error ("Mesh::loadOFF: not an OFF file: " + filename);
    unsigned int numOfVertices = 0, numOfFaces = 0, numOfEdges = 0;
    input >> numOfVertices >> numOfFaces >> numOfEdges;
    if (!input)
        throw runtime_error ("Mesh::loadOFF: malformed header in " + filename);
    vertices.reserve (numOfVertices);
    for (unsigned int i = 0; i < numOfVertices; i++) {
        Vec3Df pos;
        input >> pos;
        if (!input)
            throw runtime_error ("Mesh::loadOFF: truncated vertex list in " + filename);
        vertices.push_back (Vertex (pos));
    }
    for (unsigned int i = 0; i < numOfFaces; i++) {
        unsigned int polygonSize = 0;
        input >> polygonSize;
        if (!input || polygonSize < 3)
            throw runtime_error ("Mesh::loadOFF: bad face in " + filename);
        vector<unsigned int> index (polygonSize);
        for (unsigned int j = 0; j < polygonSize; j++) {
            input >> index[j];
            if (!input || index[j] >= numOfVertices)
                throw runtime_error ("Mesh::loadOFF: vertex index out of range in " + filename);
        }
        // Fan-triangulate: (0,1,2), (0,2,3), ...
        for (unsigned int j = 1; j + 1 < polygonSize; j++)
            triangles.push_back (Triangle (index[0], index[j], index[j+1]));
    }
    recomputeSmoothVertexNormals (0);
}
