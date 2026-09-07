#include <cmath>
#include <string>

#include "Fixtures.h"
#include "Orientation.h"
#include "Scene.h"
#include "Test.h"

namespace {

/// A square pyramid: base in the plane `axis` = 0 (4 vertices), apex along +axis.
Mesh pyramid(int axis, float sign = 1.f) {
    auto make = [&](float u, float v, float h) {
        Vec3Df p;
        p[axis] = sign * h;
        p[(axis + 1) % 3] = u;
        p[(axis + 2) % 3] = v;
        return Vertex(p);
    };
    std::vector<Vertex> verts = {make(-1.f, -1.f, 0.f), make(1.f, -1.f, 0.f), make(1.f, 1.f, 0.f),
                                 make(-1.f, 1.f, 0.f), make(0.f, 0.f, 2.f)};
    std::vector<Triangle> tris = {Triangle(0, 1, 4), Triangle(1, 2, 4), Triangle(2, 3, 4), Triangle(3, 0, 4),
                                  Triangle(0, 2, 1), Triangle(0, 3, 2)};
    Mesh m(verts, tris);
    m.recomputeSmoothVertexNormals(0);
    return m;
}

Vec3Df unit(UpAxis up) {
    switch (up) {
        case UpAxis::PosX: return Vec3Df(1.f, 0.f, 0.f);
        case UpAxis::NegX: return Vec3Df(-1.f, 0.f, 0.f);
        case UpAxis::PosY: return Vec3Df(0.f, 1.f, 0.f);
        case UpAxis::NegY: return Vec3Df(0.f, -1.f, 0.f);
        case UpAxis::PosZ: return Vec3Df(0.f, 0.f, 1.f);
        case UpAxis::NegZ: return Vec3Df(0.f, 0.f, -1.f);
    }
    return Vec3Df(0.f, 1.f, 0.f);
}

}  // namespace

TEST_CASE("orientation: names and parsing") {
    CHECK(std::string(upAxisName(UpAxis::PosZ)) == "+Z");
    CHECK(std::string(upAxisName(UpAxis::NegY)) == "-Y");
    CHECK(parseUpAxis("+z") == UpAxis::PosZ);
    CHECK(parseUpAxis("Z") == UpAxis::PosZ);
    CHECK(parseUpAxis(" -x ") == UpAxis::NegX);
    CHECK(parseUpAxis("y") == UpAxis::PosY);
    CHECK(!parseUpAxis("w").has_value());
    CHECK(!parseUpAxis("").has_value());
}

TEST_CASE("orientation: setUpAxis maps the chosen file axis onto +Y, exactly and reversibly") {
    const UpAxis all[] = {UpAxis::PosX, UpAxis::NegX, UpAxis::PosY, UpAxis::NegY, UpAxis::PosZ, UpAxis::NegZ};
    for (UpAxis up : all) {
        // A "tripod": one vertex on each positive file axis.
        std::vector<Vertex> v = {Vertex(Vec3Df(1.f, 0.f, 0.f)), Vertex(Vec3Df(0.f, 1.f, 0.f)),
                                 Vertex(Vec3Df(0.f, 0.f, 1.f))};
        Scene s = fixtures::sceneOf(Mesh(v, {Triangle(0, 1, 2)}));
        s.setUpAxis(up);
        CHECK(s.getUpAxis() == up);
        // The vertex that sat on `up` (or its opposite) now sits on +Y (or -Y).
        const Vec3Df target = unit(up);
        for (const Vertex& vert : s.getObjects()[0].getMesh().getVertices()) {
            (void)vert;
        }
        // Find where the file's `up` direction went: transform is linear, so
        // check the image of the unit vector through the moved vertices.
        Vec3Df image(0.f, 0.f, 0.f);
        for (int a = 0; a < 3; ++a) image += s.getObjects()[0].getMesh().getVertices()[a].getPos() * target[a];
        CHECK_MSG(image == Vec3Df(0.f, 1.f, 0.f), std::string("up ") + upAxisName(up));
        // Positions stay exact unit vectors (a permutation, not a rounding rotation).
        for (const Vertex& vert : s.getObjects()[0].getMesh().getVertices()) {
            CHECK_CLOSE(vert.getPos().getLength(), 1.f, 0.f);
        }
        // And back: the original coordinates return bit for bit.
        s.setUpAxis(UpAxis::PosY);
        const std::vector<Vertex>& back = s.getObjects()[0].getMesh().getVertices();
        CHECK(back[0].getPos() == Vec3Df(1.f, 0.f, 0.f));
        CHECK(back[1].getPos() == Vec3Df(0.f, 1.f, 0.f));
        CHECK(back[2].getPos() == Vec3Df(0.f, 0.f, 1.f));
    }
}

TEST_CASE("orientation: normals rotate with positions and the ground follows the new bottom") {
    Scene s = fixtures::sceneOf(pyramid(2));  // base in XY at z = 0, apex at z = 2 (a Z-up model)
    s.addGroundPlane();
    const Vec3Df apexBefore = s.getObjects()[0].getMesh().getVertices()[4].getPos();
    CHECK(apexBefore == Vec3Df(0.f, 0.f, 2.f));
    s.setUpAxis(UpAxis::PosZ);
    const Mesh& m = s.getObjects()[0].getMesh();
    CHECK(m.getVertices()[4].getPos() == Vec3Df(0.f, 2.f, 0.f));  // apex now points up
    // Bounding box follows: height 2 along Y, base square in XZ.
    CHECK_CLOSE(s.getBoundingBox().getMin()[1], 0.f, 0.f);
    CHECK_CLOSE(s.getBoundingBox().getMax()[1], 2.f, 0.f);
    CHECK_CLOSE(s.getBoundingBox().getMin()[2], -1.f, 0.f);
    // Normals were rotated too: the apex's normal now points along +Y.
    Vec3Df apexNormal = m.getVertices()[4].getNormal();
    apexNormal.normalize();
    CHECK(apexNormal[1] > 0.99f);
    // The ground was rebuilt under the new bottom (y = 0) and is still a backdrop.
    REQUIRE(s.getObjects().size() == 2);
    CHECK(s.getObjects()[1].isBackdrop());
    for (const Vertex& v : s.getObjects()[1].getMesh().getVertices()) {
        CHECK_CLOSE(v.getPos()[1], 0.f, 1e-6);
        CHECK(v.getNormal() == Vec3Df(0.f, 1.f, 0.f));
    }
    CHECK_CLOSE(s.getBoundingBox().getMax()[1], 2.f, 0.f);  // the box still ignores the backdrop
}

TEST_CASE("orientation: the flattest-side heuristic finds a pyramid's base") {
    CHECK(detectUpAxis(fixtures::sceneOf(pyramid(2))) == UpAxis::PosZ);        // base at z = 0, apex +z
    CHECK(detectUpAxis(fixtures::sceneOf(pyramid(1))) == UpAxis::PosY);        // base at y = 0, apex +y
    CHECK(detectUpAxis(fixtures::sceneOf(pyramid(0))) == UpAxis::PosX);        // base at x = 0, apex +x
    CHECK(detectUpAxis(fixtures::sceneOf(pyramid(2, -1.f))) == UpAxis::NegZ);  // apex at -z: base on top
    CHECK(detectUpAxis(fixtures::sceneOf(fixtures::cube(Vec3Df(0.f, 0.f, 0.f), 2.f))) == UpAxis::PosY);  // tie
}

TEST_CASE("orientation: the manifest wins over the heuristic and unknown models fall back") {
    fixtures::writeFile("orientation.txt", "# name  up\nzmodel -z\nxmodel x   # trailing comment\nbad w\n");
    CHECK(upAxisFromManifest(test::outputDir() + "/zmodel.off") == UpAxis::NegZ);
    CHECK(upAxisFromManifest(test::outputDir() + "/xmodel.obj") == UpAxis::PosX);
    CHECK(!upAxisFromManifest(test::outputDir() + "/bad.off").has_value());      // unparsable axis
    CHECK(!upAxisFromManifest(test::outputDir() + "/unknown.off").has_value());  // no entry
    CHECK(!upAxisFromManifest("/nowhere/at/all/model.off").has_value());         // no manifest

    const Scene pyr = fixtures::sceneOf(pyramid(2));
    std::string source;
    CHECK(resolveUpAxis(test::outputDir() + "/zmodel.off", pyr, &source) == UpAxis::NegZ);
    CHECK(source == "orientation.txt");
    CHECK(resolveUpAxis(test::outputDir() + "/unknown.off", pyr, &source) == UpAxis::PosZ);
    CHECK(source == "heuristic");
}

TEST_CASE("orientation: bundled ram and teapot are Z-up and stand on their ground") {
    for (const char* name : {"ram", "teapot"}) {
        Scene s;
        s.addObjectsFromFile(test::modelPath(name));
        std::string source;
        const UpAxis up = resolveUpAxis(test::modelPath(name), s, &source);
        CHECK_MSG(up == UpAxis::PosZ, name);
        CHECK_MSG(source == "orientation.txt", name);
        const float heightInFile = s.getBoundingBox().getMax()[2] - s.getBoundingBox().getMin()[2];
        s.setUpAxis(up);
        s.addGroundPlane();
        // Height is now along Y, the floor is at the bottom, feet/base on it.
        CHECK_CLOSE(s.getBoundingBox().getMax()[1] - s.getBoundingBox().getMin()[1], heightInFile, 1e-5);
        const float floor = s.getBoundingBox().getMin()[1];
        const Mesh& ground = s.getObjects().back().getMesh();
        CHECK_CLOSE(ground.getVertices()[0].getPos()[1], floor, 1e-6);
    }
}
