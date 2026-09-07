// ---------------------------------------------------------
// Mesh Class
// Author : Tamy Boubekeur (boubek@gmail.com).
// Copyright (C) 2008 Tamy Boubekeur.
// All rights reserved.
// ---------------------------------------------------------

#ifndef MESH_H
#define MESH_H

#include <vector>
#include <string>

#include "Vertex.h"
#include "Triangle.h"
#include "Edge.h"

class Mesh {
public:
    inline Mesh () {}
    inline Mesh (const std::vector<Vertex> & v)
        : vertices (v) {}
    inline Mesh (const std::vector<Vertex> & v,
                 const std::vector<Triangle> & t)
        : vertices (v), triangles (t) {}
    virtual ~Mesh () {}

    std::vector<Vertex> & getVertices () { return vertices; }
    const std::vector<Vertex> & getVertices () const { return vertices; }
    std::vector<Triangle> & getTriangles () { return triangles; }
    const std::vector<Triangle> & getTriangles () const { return triangles; }

    void clear ();
    void clearGeometry ();
    void clearTopology ();
    void unmarkAllVertices ();

    // weight: 0 = uniform, 1 = triangle area, 2 = corner angle.
    void recomputeSmoothVertexNormals (unsigned int weight);
    void computeTriangleNormals (std::vector<Vec3Df> & triangleNormals) const;

    // Adjacency helpers (unused by the raytracer today, kept for mesh
    // processing experiments such as smoothing or subdivision).
    void collectOneRing (std::vector<std::vector<unsigned int> > & oneRing) const;
    void collectOrderedOneRing (std::vector<std::vector<unsigned int> > & oneRing) const;
    void computeDualEdgeMap (EdgeMapIndex & dualVMap1, EdgeMapIndex & dualVMap2);
    void markBorderEdges (EdgeMapIndex & edgeMap);

    // Load an ASCII OFF file. Polygons with more than three vertices are
    // fan-triangulated and smooth vertex normals are recomputed. Extra
    // columns after a vertex or after a face's indices (colours) and '#'
    // comment lines are ignored; the counts may sit on the "OFF" line.
    // Throws std::runtime_error if the file is missing or malformed.
    void loadOFF (const std::string & filename);

private:
    std::vector<Vertex> vertices;
    std::vector<Triangle> triangles;
};

#endif // MESH_H
