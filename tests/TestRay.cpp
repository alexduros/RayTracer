#include "Fixtures.h"
#include "Ray.h"
#include "Test.h"

namespace {
Mesh triangleZ0() {
    // CCW seen from +Z -> geometric normal +Z; vertex normals set to +Z too.
    const Vec3Df n(0.f, 0.f, 1.f);
    std::vector<Vertex> v = {Vertex(Vec3Df(0.f, 0.f, 0.f), n), Vertex(Vec3Df(1.f, 0.f, 0.f), n), Vertex(Vec3Df(0.f, 1.f, 0.f), n)};
    return Mesh(v, {Triangle(0, 1, 2)});
}
}  // namespace

TEST_CASE("ray: triangle hit gives distance, position and interpolated normal") {
    const Mesh m = triangleZ0();
    Ray r(Vec3Df(0.2f, 0.3f, 2.f), Vec3Df(0.f, 0.f, -1.f));
    Vertex hit;
    float t = -1.f;
    REQUIRE(r.hit(m.getTriangles()[0], m, hit, t));
    CHECK_CLOSE(t, 2.f, 1e-5);
    CHECK_CLOSE(hit.getPos()[0], 0.2f, 1e-5);
    CHECK_CLOSE(hit.getPos()[1], 0.3f, 1e-5);
    CHECK_CLOSE(hit.getPos()[2], 0.f, 1e-5);
    CHECK_CLOSE(hit.getNormal()[2], 1.f, 1e-5);
}

TEST_CASE("ray: misses outside the triangle, behind the origin and on back faces") {
    const Mesh m = triangleZ0();
    Vertex hit;
    float t = 0.f;
    CHECK_MSG(!Ray(Vec3Df(0.7f, 0.7f, 1.f), Vec3Df(0.f, 0.f, -1.f)).hit(m.getTriangles()[0], m, hit, t), "outside (u+v>1)");
    CHECK_MSG(!Ray(Vec3Df(-0.1f, 0.2f, 1.f), Vec3Df(0.f, 0.f, -1.f)).hit(m.getTriangles()[0], m, hit, t), "outside (u<0)");
    CHECK_MSG(!Ray(Vec3Df(0.2f, 0.2f, 1.f), Vec3Df(0.f, 0.f, 1.f)).hit(m.getTriangles()[0], m, hit, t), "pointing away");
    CHECK_MSG(!Ray(Vec3Df(0.2f, 0.2f, -1.f), Vec3Df(0.f, 0.f, 1.f)).hit(m.getTriangles()[0], m, hit, t), "back face is culled");
    CHECK_MSG(!Ray(Vec3Df(0.2f, 0.2f, 1.f), Vec3Df(1.f, 0.f, 0.f)).hit(m.getTriangles()[0], m, hit, t), "parallel to the plane");
}

TEST_CASE("ray: nearestHit returns the closest triangle whatever the order") {
    // Two quads facing +Z at z = 0 and z = -1; the ray comes from z = +1.
    const Vec3Df n(0.f, 0.f, 1.f);
    std::vector<Vertex> v;
    for (float z : {0.f, -1.f}) {
        v.push_back(Vertex(Vec3Df(-1.f, -1.f, z), n));
        v.push_back(Vertex(Vec3Df(1.f, -1.f, z), n));
        v.push_back(Vertex(Vec3Df(1.f, 1.f, z), n));
        v.push_back(Vertex(Vec3Df(-1.f, 1.f, z), n));
    }
    const std::vector<Triangle> nearFirst = {Triangle(0, 1, 2), Triangle(0, 2, 3), Triangle(4, 5, 6), Triangle(4, 6, 7)};
    const std::vector<Triangle> farFirst = {Triangle(4, 5, 6), Triangle(4, 6, 7), Triangle(0, 1, 2), Triangle(0, 2, 3)};
    const Ray r(Vec3Df(0.1f, 0.1f, 1.f), Vec3Df(0.f, 0.f, -1.f));
    for (const auto* order : {&nearFirst, &farFirst}) {
        Mesh m(v, *order);
        Vertex hit;
        float t = 0.f;
        REQUIRE(r.nearestHit(m, hit, t));
        CHECK_CLOSE(t, 1.f, 1e-5);
        CHECK_CLOSE(hit.getPos()[2], 0.f, 1e-5);   // the near quad, not the last one tested
    }
}

TEST_CASE("ray: nearestHit leaves outputs untouched on a miss") {
    const Mesh m = triangleZ0();
    Vertex hit;
    hit.setPos(Vec3Df(9.f, 9.f, 9.f));
    float t = 42.f;
    CHECK(!Ray(Vec3Df(5.f, 5.f, 1.f), Vec3Df(0.f, 0.f, -1.f)).nearestHit(m, hit, t));
    CHECK_CLOSE(t, 42.f, 0.f);
    CHECK(hit.getPos() == Vec3Df(9.f, 9.f, 9.f));
}

TEST_CASE("ray: bounding box slab test") {
    const BoundingBox box(Vec3Df(-1.f, -1.f, -1.f), Vec3Df(1.f, 1.f, 1.f));
    Vec3Df p;
    CHECK(Ray(Vec3Df(0.f, 0.f, 5.f), Vec3Df(0.f, 0.f, -1.f)).intersect(box, p));
    CHECK_CLOSE(p[2], 1.f, 1e-5);   // entry point on the +z face
    CHECK_MSG(Ray(Vec3Df(0.f, 0.f, 0.f), Vec3Df(1.f, 0.f, 0.f)).intersect(box), "origin inside");
    CHECK_MSG(!Ray(Vec3Df(0.f, 0.f, 5.f), Vec3Df(0.f, 0.f, 1.f)).intersect(box), "pointing away");
    CHECK_MSG(!Ray(Vec3Df(3.f, 0.f, 5.f), Vec3Df(0.f, 0.f, -1.f)).intersect(box), "passes beside the box");
}
