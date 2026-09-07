// ---------------------------------------------------------
// Mesh Class
// Author : Tamy Boubekeur (boubek@gmail.com).
// Copyright (C) 2008 Tamy Boubekeur.
// All rights reserved.
// ---------------------------------------------------------

#include "Mesh.h"
#include <algorithm>
#include <fstream>
#include <sstream>
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

namespace {

/// Next line that is neither blank nor a '#' comment, CR stripped.
bool nextContentLine (std::istream & in, std::string & line) {
    while (std::getline (in, line)) {
        if (!line.empty () && line.back () == '\r')
            line.pop_back ();
        const size_t first = line.find_first_not_of (" \t");
        if (first == std::string::npos || line[first] == '#')
            continue;
        return true;
    }
    return false;
}

} // namespace

void Mesh::loadOFF (const std::string & filename) {
    clear ();
    ifstream input (filename.c_str ());
    if (!input)
        throw runtime_error ("Mesh::loadOFF: cannot open " + filename);
    // Line based on purpose: OFF allows extra columns after a vertex (colour,
    // normal) and after a face's indices (a colour), which a token stream
    // would read as the next vertex or face.
    string line;
    if (!nextContentLine (input, line))
        throw runtime_error ("Mesh::loadOFF: empty file " + filename);
    istringstream header (line);
    string magic_word;
    header >> magic_word;
    if (magic_word != "OFF")
        throw runtime_error ("Mesh::loadOFF: not an OFF file: " + filename);
    // Counts follow either on the same line ("OFF 8 6 0") or on the next one.
    unsigned int numOfVertices = 0, numOfFaces = 0, numOfEdges = 0;
    if (header >> numOfVertices >> numOfFaces) {
        header >> numOfEdges;
    } else {
        if (!nextContentLine (input, line))
            throw runtime_error ("Mesh::loadOFF: malformed header in " + filename);
        istringstream counts (line);
        if (!(counts >> numOfVertices >> numOfFaces))
            throw runtime_error ("Mesh::loadOFF: malformed header in " + filename);
        counts >> numOfEdges;  // optional
    }
    // One vertex per line: x y z, anything after is ignored.
    vertices.reserve (numOfVertices);
    for (unsigned int i = 0; i < numOfVertices; i++) {
        if (!nextContentLine (input, line))
            throw runtime_error ("Mesh::loadOFF: truncated vertex list in " + filename);
        istringstream ss (line);
        Vec3Df pos;
        if (!(ss >> pos))
            throw runtime_error ("Mesh::loadOFF: malformed vertex in " + filename);
        vertices.push_back (Vertex (pos));
    }
    // One face per line: n i0 .. i(n-1), anything after (a colour) is ignored.
    for (unsigned int i = 0; i < numOfFaces; i++) {
        if (!nextContentLine (input, line))
            throw runtime_error ("Mesh::loadOFF: truncated face list in " + filename);
        istringstream ss (line);
        unsigned int polygonSize = 0;
        if (!(ss >> polygonSize) || polygonSize < 3)
            throw runtime_error ("Mesh::loadOFF: bad face in " + filename);
        vector<unsigned int> index (polygonSize);
        for (unsigned int j = 0; j < polygonSize; j++) {
            if (!(ss >> index[j]) || index[j] >= numOfVertices)
                throw runtime_error ("Mesh::loadOFF: vertex index out of range in " + filename);
        }
        // Fan-triangulate: (0,1,2), (0,2,3), ...
        for (unsigned int j = 1; j + 1 < polygonSize; j++)
            triangles.push_back (Triangle (index[0], index[j], index[j+1]));
    }
    recomputeSmoothVertexNormals (0);
}
