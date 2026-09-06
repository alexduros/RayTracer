// Small synthetic meshes and scenes shared by the tests.
#pragma once

#include <fstream>
#include <string>
#include <vector>

#include "Material.h"
#include "Mesh.h"
#include "Scene.h"
#include "Test.h"
#include "Vec3D.h"

namespace fixtures {

/// Two CCW triangles in the plane z = z0 covering [-half, half]^2, normal +Z.
inline Mesh quad(float z0, float half) {
    const Vec3Df n(0.f, 0.f, 1.f);
    std::vector<Vertex> v = {
        Vertex(Vec3Df(-half, -half, z0), n), Vertex(Vec3Df(half, -half, z0), n),
        Vertex(Vec3Df(half, half, z0), n),   Vertex(Vec3Df(-half, half, z0), n)};
    std::vector<Triangle> t = {Triangle(0, 1, 2), Triangle(0, 2, 3)};
    return Mesh(v, t);
}

/// Axis-aligned cube of side `side` centred at `c`, faces wound CCW seen from
/// outside, smooth vertex normals recomputed (so they point along diagonals).
inline Mesh cube(const Vec3Df& c, float side) {
    const float h = side / 2.f;
    std::vector<Vertex> v;
    for (int i = 0; i < 8; ++i) {
        v.push_back(Vertex(c + Vec3Df((i & 1) ? h : -h, (i & 2) ? h : -h, (i & 4) ? h : -h)));
    }
    // vertex i: bit0 = +x, bit1 = +y, bit2 = +z
    std::vector<Triangle> t = {
        Triangle(4, 5, 7), Triangle(5, 7 ^ 0, 7),  // placeholder, replaced below
    };
    t = {
        Triangle(4, 5, 7), Triangle(5, 7, 6),  // +z? no: (4,5,7) -> fix below
    };
    // Explicit list (indices: 0=(-,-,-) 1=(+,-,-) 2=(-,+,-) 3=(+,+,-) 4=(-,-,+) 5=(+,-,+) 6=(-,+,+) 7=(+,+,+))
    t = {
        Triangle(4, 5, 7), Triangle(4, 7, 6),  // front  z = +h
        Triangle(1, 0, 2), Triangle(1, 2, 3),  // back   z = -h
        Triangle(5, 1, 3), Triangle(5, 3, 7),  // right  x = +h
        Triangle(0, 4, 6), Triangle(0, 6, 2),  // left   x = -h
        Triangle(6, 7, 3), Triangle(6, 3, 2),  // top    y = +h
        Triangle(0, 1, 5), Triangle(0, 5, 4),  // bottom y = -h
    };
    Mesh m(v, t);
    m.recomputeSmoothVertexNormals(0);
    return m;
}

/// Write a text file into the test output directory and return its path.
inline std::string writeFile(const std::string& name, const std::string& body) {
    const std::string path = test::outputDir() + "/" + name;
    std::ofstream out(path);
    out << body;
    return path;
}

/// Same as writeFile; the name the mesh tests use for their OFF fixtures.
inline std::string writeOFF(const std::string& name, const std::string& body) { return writeFile(name, body); }

inline Material white() { return Material(1.f, 0.f, Vec3Df(1.f, 1.f, 1.f)); }

inline Scene sceneOf(const Mesh& m, const Material& mat = white()) {
    Scene s;
    s.addObject(Object(m, mat));
    return s;
}

}  // namespace fixtures
