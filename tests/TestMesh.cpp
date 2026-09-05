#include <stdexcept>

#include "Fixtures.h"
#include "Object.h"
#include "Test.h"

TEST_CASE("mesh: loadOFF triangulates quads and computes unit normals") {
    const std::string path = fixtures::writeOFF("quad.off",
        "OFF\n4 1 0\n"
        "-1 -1 0\n 1 -1 0\n 1 1 0\n-1 1 0\n"
        "4 0 1 2 3\n");
    Mesh m;
    m.loadOFF(path);
    CHECK_EQ(m.getVertices().size(), 4u);
    CHECK_EQ(m.getTriangles().size(), 2u);   // one quad -> two triangles
    CHECK(m.getTriangles()[0] == Triangle(0, 1, 2));
    CHECK(m.getTriangles()[1] == Triangle(0, 2, 3));
    for (const Vertex& v : m.getVertices()) {
        CHECK_CLOSE(v.getNormal().getLength(), 1.f, 1e-5);
        CHECK_CLOSE(v.getNormal()[2], 1.f, 1e-5);   // CCW in XY -> +Z
    }
}

TEST_CASE("mesh: loadOFF reports missing or malformed files") {
    Mesh m;
    bool threw = false;
    try { m.loadOFF(test::outputDir() + "/does_not_exist.off"); } catch (const std::runtime_error&) { threw = true; }
    CHECK_MSG(threw, "missing file");

    threw = false;
    try { m.loadOFF(fixtures::writeOFF("notoff.off", "PLY\n0 0 0\n")); } catch (const std::runtime_error&) { threw = true; }
    CHECK_MSG(threw, "wrong magic word");

    threw = false;
    try { m.loadOFF(fixtures::writeOFF("badindex.off", "OFF\n3 1 0\n0 0 0\n1 0 0\n0 1 0\n3 0 1 9\n")); } catch (const std::runtime_error&) { threw = true; }
    CHECK_MSG(threw, "vertex index out of range");

    threw = false;
    try { m.loadOFF(fixtures::writeOFF("truncated.off", "OFF\n3 1 0\n0 0 0\n1 0 0\n")); } catch (const std::runtime_error&) { threw = true; }
    CHECK_MSG(threw, "truncated vertex list");
}

TEST_CASE("mesh: bundled teapot has the expected size") {
    Mesh m;
    m.loadOFF(test::modelPath("teapot"));
    CHECK_EQ(m.getVertices().size(), 480u);
    CHECK_EQ(m.getTriangles().size(), 880u);   // 432 quads * 2 + 16 triangles
    for (const Triangle& t : m.getTriangles())
        for (unsigned int i = 0; i < 3; ++i)
            CHECK(t.getVertex(i) < m.getVertices().size());
}

TEST_CASE("mesh: object and scene bounding boxes") {
    Object o(fixtures::cube(Vec3Df(1.f, 2.f, 3.f), 2.f), fixtures::white());
    const BoundingBox& b = o.getBoundingBox();
    CHECK(b.getMin() == Vec3Df(0.f, 1.f, 2.f));
    CHECK(b.getMax() == Vec3Df(2.f, 3.f, 4.f));
    CHECK(b.getCenter() == Vec3Df(1.f, 2.f, 3.f));
    CHECK_CLOSE(b.getSize(), 2.f, 1e-6);
    CHECK(b.contains(Vec3Df(1.f, 2.f, 3.f)));
    CHECK(!b.contains(Vec3Df(5.f, 2.f, 3.f)));

    Scene s;
    s.addObject(o);
    s.addObject(Object(fixtures::quad(-5.f, 1.f), fixtures::white()));
    CHECK(s.getBoundingBox().getMin() == Vec3Df(-1.f, -1.f, -5.f));
    CHECK(s.getBoundingBox().getMax() == Vec3Df(2.f, 3.f, 4.f));
    CHECK_EQ(s.getBoundingBox().getDirection(), 2);   // z is the longest axis
}

TEST_CASE("mesh: default lights are placed relative to the model") {
    Scene s = fixtures::sceneOf(fixtures::cube(Vec3Df(10.f, 0.f, 0.f), 4.f));
    s.addDefaultLights();
    REQUIRE(s.getLights().size() == 3);
    // key light: centre + (size/2) * (3,3,3)
    CHECK(s.getLights()[0].getPos() == Vec3Df(16.f, 6.f, 6.f));
    CHECK_CLOSE(s.getLights()[0].getRadius(), 6.f, 1e-6);
}
