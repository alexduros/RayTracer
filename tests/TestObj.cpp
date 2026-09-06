#include <cmath>
#include <functional>
#include <stdexcept>
#include <string>

#include "Camera.h"
#include "Fixtures.h"
#include "ObjLoader.h"
#include "RayTracer.h"
#include "Scene.h"
#include "Test.h"

namespace {

const float kPi = 3.14159265358979f;

bool throwsRuntimeError(const std::function<void()>& fn) {
    try {
        fn();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

struct RGB {
    int r, g, b;
};
RGB px(const Image& img, int x, int y) {
    unsigned char r, g, b;
    img.getPixel(x, y, r, g, b);
    return {r, g, b};
}

/// A quad facing +Z (4 positions, 4 texcoords, 1 normal) with the given face line.
std::string quadObj(const std::string& faceLine, const std::string& header = "") {
    return header +
           "v -1 -1 0\nv 1 -1 0\nv 1 1 0\nv -1 1 0\n"
           "vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
           "vn 0 0 1\n" +
           faceLine + "\n";
}

}  // namespace

TEST_CASE("obj: MTL parsing maps Kd to colour and mean Ks to specular") {
    const std::string path = fixtures::writeFile("two.mtl",
        "# comment\n"
        "newmtl red\n"
        "Kd 0.9 0.1 0.2\n"
        "Ks 0.3 0.6 0.9\n"
        "Ns 50\n"
        "map_Kd nothing.png\n"
        "\n"
        "newmtl plain\n");
    const std::map<std::string, Material> materials = loadMTL(path);
    REQUIRE(materials.size() == 2);
    const Material& red = materials.at("red");
    CHECK(red.getColor() == Vec3Df(0.9f, 0.1f, 0.2f));
    CHECK_CLOSE(red.getSpecular(), 0.6f, 1e-6);
    CHECK_CLOSE(red.getDiffuse(), 1.f, 1e-6);
    const Material& plain = materials.at("plain");
    CHECK(plain.getColor() == Vec3Df(0.8f, 0.8f, 0.8f));  // default when Kd is absent
    CHECK_CLOSE(plain.getSpecular(), 0.f, 1e-6);
    CHECK(throwsRuntimeError([] { loadMTL(test::outputDir() + "/missing.mtl"); }));
}

TEST_CASE("obj: bundled cube gives one object per material with the file's normals") {
    Scene scene;
    CHECK_EQ(scene.addObjectsFromFile(test::modelPath("cube.obj")), 6u);
    REQUIRE(scene.getObjects().size() == 6);
    size_t vertices = 0, triangles = 0;
    for (const Object& o : scene.getObjects()) {
        CHECK_EQ(o.getMesh().getVertices().size(), 4u);   // de-duplicated on (position, normal)
        CHECK_EQ(o.getMesh().getTriangles().size(), 2u);  // one quad, fan-triangulated
        vertices += o.getMesh().getVertices().size();
        triangles += o.getMesh().getTriangles().size();
    }
    CHECK_EQ(vertices, 24u);
    CHECK_EQ(triangles, 12u);
    // Objects come in first-use order: front, back, right, left, top, bottom.
    const Object& front = scene.getObjects()[0];
    const Object& top = scene.getObjects()[4];
    CHECK(front.getMaterial().getColor() == Vec3Df(0.9f, 0.2f, 0.2f));
    CHECK_CLOSE(front.getMaterial().getSpecular(), 0.3f, 1e-6);
    CHECK(top.getMaterial().getColor() == Vec3Df(0.9f, 0.9f, 0.9f));
    for (const Vertex& v : front.getMesh().getVertices()) CHECK(v.getNormal() == Vec3Df(0.f, 0.f, 1.f));
    for (const Vertex& v : top.getMesh().getVertices()) CHECK(v.getNormal() == Vec3Df(0.f, 1.f, 0.f));
    CHECK_CLOSE(scene.getBoundingBox().getSize(), 1.f, 1e-6);
    CHECK(scene.getBoundingBox().getCenter() == Vec3Df(0.f, 0.f, 0.f));
}

TEST_CASE("obj: every face syntax and negative indices give the same geometry") {
    const char* faces[] = {
        "f 1 2 3 4",
        "f 1/1 2/2 3/3 4/4",
        "f 1//1 2//1 3//1 4//1",
        "f 1/1/1 2/2/1 3/3/1 4/4/1",
        "f -4/-4/-1 -3/-3/-1 -2/-2/-1 -1/-1/-1",
    };
    for (const char* face : faces) {
        const std::string path = fixtures::writeFile("quad_syntax.obj", quadObj(face));
        const std::vector<Object> objects = loadOBJ(path, fixtures::white());
        REQUIRE(objects.size() == 1);
        const Mesh& m = objects[0].getMesh();
        CHECK_MSG(m.getVertices().size() == 4, face);
        CHECK_MSG(m.getTriangles().size() == 2, face);
        CHECK_MSG(m.getTriangles()[0] == Triangle(0, 1, 2) && m.getTriangles()[1] == Triangle(0, 2, 3), face);
        CHECK_MSG(m.getVertices()[2].getPos() == Vec3Df(1.f, 1.f, 0.f), face);
        // Normal from the file when given, recomputed (CCW in XY -> +Z) otherwise.
        for (const Vertex& v : m.getVertices()) CHECK_MSG(std::fabs(v.getNormal()[2] - 1.f) < 1e-5f, face);
    }
}

TEST_CASE("obj: materials switch per face and non-contiguous runs merge") {
    fixtures::writeFile("two_runs.mtl", "newmtl a\nKd 1 0 0\nnewmtl b\nKd 0 0 1\n");
    const std::string path = fixtures::writeFile("two_runs.obj",
        "mtllib two_runs.mtl\n"
        "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\n"
        "v 0 0 1\nv 1 0 1\nv 1 1 1\nv 0 1 1\n"
        "usemtl a\nf 1 2 3\n"
        "usemtl b\nf 5 6 7 8\n"
        "usemtl a\nf 1 3 4\n");
    const std::vector<Object> objects = loadOBJ(path, fixtures::white());
    REQUIRE(objects.size() == 2);
    CHECK_EQ(objects[0].getMesh().getTriangles().size(), 2u);  // both 'a' runs in one object
    CHECK_EQ(objects[0].getMesh().getVertices().size(), 4u);   // vertices 1 and 3 shared
    CHECK(objects[0].getMaterial().getColor() == Vec3Df(1.f, 0.f, 0.f));
    CHECK_EQ(objects[1].getMesh().getTriangles().size(), 2u);
    CHECK(objects[1].getMaterial().getColor() == Vec3Df(0.f, 0.f, 1.f));
}

TEST_CASE("obj: missing MTL and unknown material fall back to the given material") {
    const Material fallback(1.f, 0.f, Vec3Df(0.1f, 0.2f, 0.3f));
    const std::string path =
        fixtures::writeFile("nomtl.obj", quadObj("f 1 2 3 4", "mtllib does_not_exist.mtl\nusemtl ghost\n"));
    const std::vector<Object> objects = loadOBJ(path, fallback);  // warns on stderr, must not throw
    REQUIRE(objects.size() == 1);
    CHECK(objects[0].getMaterial().getColor() == Vec3Df(0.1f, 0.2f, 0.3f));
}

TEST_CASE("obj: malformed files and unsupported formats are reported") {
    const Material white = fixtures::white();
    CHECK_MSG(throwsRuntimeError([&] { loadOBJ(test::outputDir() + "/missing.obj", white); }), "missing file");
    CHECK_MSG(throwsRuntimeError([&] {
        loadOBJ(fixtures::writeFile("badindex.obj", "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 9\n"), white);
    }), "index out of range");
    CHECK_MSG(throwsRuntimeError([&] {
        loadOBJ(fixtures::writeFile("zeroindex.obj", "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 0 1 2\n"), white);
    }), "index 0 is invalid in OBJ");
    CHECK_MSG(throwsRuntimeError([&] {
        loadOBJ(fixtures::writeFile("nofaces.obj", "v 0 0 0\nv 1 0 0\nv 0 1 0\n"), white);
    }), "no faces");
    CHECK_MSG(throwsRuntimeError([&] {
        loadOBJ(fixtures::writeFile("garbage.obj", "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 x 3\n"), white);
    }), "non-numeric index");

    Scene scene;
    CHECK_MSG(throwsRuntimeError([&] { scene.addObjectsFromFile(fixtures::writeFile("mesh.stl", "solid nope\n")); }),
              "unsupported extension");
    // Dispatch on the extension is case-insensitive.
    CHECK_EQ(scene.addObjectsFromFile(fixtures::writeFile(
                 "quad_upper.OFF", "OFF\n4 1 0\n-1 -1 0\n1 -1 0\n1 1 0\n-1 1 0\n4 0 1 2 3\n")),
             1u);
    CHECK_EQ(scene.addObjectsFromFile(fixtures::writeFile("quad_upper.OBJ", quadObj("f 1 2 3 4"))), 1u);
    CHECK_EQ(scene.getObjects().size(), 2u);
}

TEST_CASE("obj: rendering uses each face's material") {
    Scene scene;
    scene.addObjectsFromFile(test::modelPath("cube.obj"));
    const BoundingBox& bbox = scene.getBoundingBox();
    const Camera fromFront = Camera::frame(bbox, kPi / 4.f, 1.f);
    const Camera fromAbove = Camera::frame(bbox, kPi / 4.f, 1.f, 2.f, 0.f, 89.f);
    RayTracer rt;

    rt.setDebugMode(RayTracer::DebugMode::AMBIENT);
    RGB c = px(rt.render(scene, fromFront, 32, 32), 16, 16);  // front face: Kd 0.9 0.2 0.2
    CHECK_CLOSE(c.r, 230, 1);
    CHECK_CLOSE(c.g, 51, 1);
    CHECK_CLOSE(c.b, 51, 1);
    c = px(rt.render(scene, fromAbove, 32, 32), 16, 16);  // top face: Kd 0.9 0.9 0.9
    CHECK_CLOSE(c.r, 230, 1);
    CHECK_CLOSE(c.g, 230, 1);
    CHECK_CLOSE(c.b, 230, 1);

    rt.setDebugMode(RayTracer::DebugMode::OBJECT_ID);
    const RGB front = px(rt.render(scene, fromFront, 32, 32), 16, 16);
    const RGB top = px(rt.render(scene, fromAbove, 32, 32), 16, 16);
    CHECK(front.r != top.r || front.g != top.g || front.b != top.b);
}
