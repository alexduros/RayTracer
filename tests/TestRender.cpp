#include <cstring>

#include "Camera.h"
#include "Fixtures.h"
#include "RayTracer.h"
#include "Test.h"

namespace {

const float kPi = 3.14159265358979f;

struct RGB {
    int r, g, b;
    bool operator==(const RGB& o) const { return r == o.r && g == o.g && b == o.b; }
    bool operator!=(const RGB& o) const { return !(*this == o); }
};
std::ostream& operator<<(std::ostream& s, const RGB& c) { return s << "(" << c.r << "," << c.g << "," << c.b << ")"; }

RGB px(const Image& img, int x, int y) {
    unsigned char r, g, b;
    img.getPixel(x, y, r, g, b);
    return {r, g, b};
}

const RGB kBlack{0, 0, 0};
const RGB kWhite{255, 255, 255};

/// Camera on +Z at height z looking at the origin (45 degree vertical fov, square).
Camera frontCamera(float z) {
    return Camera::lookAt(Vec3Df(0.f, 0.f, z), Vec3Df(0.f, 0.f, 0.f), Vec3Df(0.f, 1.f, 0.f), kPi / 4.f, 1.f);
}

unsigned long countNonBlack(const Image& img) {
    unsigned long n = 0;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
            if (px(img, x, y) != kBlack) ++n;
    return n;
}

}  // namespace

TEST_CASE("render: hit mask of a cube, hit statistics") {
    Scene s = fixtures::sceneOf(fixtures::cube(Vec3Df(0.f, 0.f, 0.f), 2.f));
    RayTracer rt;
    rt.setDebugMode(RayTracer::DebugMode::HIT_MASK);
    const Camera cam = Camera::frame(s.getBoundingBox(), kPi / 4.f, 1.f);   // eye at (0,0,4)
    const Image img = rt.render(s, cam, 64, 64);

    CHECK_EQ(px(img, 32, 32), kWhite);
    CHECK_EQ(px(img, 0, 0), kBlack);
    CHECK_EQ(px(img, 63, 63), kBlack);

    const RayTracer::Stats& st = rt.getLastStats();
    CHECK_EQ(st.rays, 4096ul);
    CHECK_EQ(st.hits, countNonBlack(img));
    // Only the front face (z = 1, 3 units away) is visible: at that depth the
    // frame spans +-3*tan(22.5deg) = +-1.243, so the face covers (2/2.485)^2 = 65%.
    const double fraction = double(st.hits) / double(st.rays);
    CHECK_MSG(fraction > 0.60 && fraction < 0.70, "hit fraction " + std::to_string(fraction));
    CHECK_CLOSE(st.minHitDist, 3.f, 1e-3);
    CHECK(st.maxHitDist > 3.f && st.maxHitDist < 3.4f);
    CHECK(st.seconds >= 0.0);
}

TEST_CASE("render: normals mode encodes (n + 1) / 2") {
    Scene s = fixtures::sceneOf(fixtures::quad(0.f, 10.f));   // faces +Z
    RayTracer rt;
    rt.setDebugMode(RayTracer::DebugMode::NORMALS);
    const Image img = rt.render(s, frontCamera(5.f), 16, 16);
    const RGB c = px(img, 8, 8);
    CHECK_CLOSE(c.r, 128, 1);
    CHECK_CLOSE(c.g, 128, 1);
    CHECK_EQ(c.b, 255);
}

TEST_CASE("render: depth mode and closest object across objects") {
    // Small quad at z = 0 in front of a large quad at z = -2, camera at z = 1.
    // The near quad is added *second*: closestHit must pick it anyway.
    Scene s;
    s.addObject(Object(fixtures::quad(-2.f, 10.f), fixtures::white()));
    s.addObject(Object(fixtures::quad(0.f, 0.2f), fixtures::white()));
    RayTracer rt;
    rt.setDebugMode(RayTracer::DebugMode::DEPTH);
    rt.setDepthRange(1.f, 3.f);
    const Image img = rt.render(s, frontCamera(1.f), 32, 32);
    CHECK_EQ(px(img, 16, 16), (RGB{255, 255, 255}));  // distance 1 -> t = 0 -> white (near)
    CHECK_EQ(px(img, 0, 0), (RGB{51, 51, 51}));       // far quad, distance > 3 -> clamped -> dark grey
    CHECK_CLOSE(rt.getLastStats().minHitDist, 1.f, 1e-3);
}

TEST_CASE("render: Lambert lighting") {
    const Material white(1.f, 0.f, Vec3Df(1.f, 1.f, 1.f));
    RayTracer rt;
    rt.setDebugMode(RayTracer::DebugMode::LIT);
    rt.setAmbientIntensity(0.f);

    // Light straight above the surface: n.l = 1 -> full white.
    Scene head = fixtures::sceneOf(fixtures::quad(0.f, 10.f), white);
    head.addLight(Light(Vec3Df(0.f, 0.f, 10.f), Vec3Df(1.f, 1.f, 1.f), 1.f, 1.f));
    RGB c = px(rt.render(head, frontCamera(5.f), 16, 16), 8, 8);
    CHECK_MSG(c.r >= 254 && c.g >= 254 && c.b >= 254, "n.l = 1 gives white, got " + std::to_string(c.r));

    // Grazing light: n.l ~ 0 -> black.
    Scene grazing = fixtures::sceneOf(fixtures::quad(0.f, 10.f), white);
    grazing.addLight(Light(Vec3Df(100.f, 0.f, 0.001f), Vec3Df(1.f, 1.f, 1.f), 1.f, 1.f));
    c = px(rt.render(grazing, frontCamera(5.f), 16, 16), 8, 8);
    CHECK_MSG(c.r <= 1 && c.g <= 1 && c.b <= 1, "grazing light gives black");

    // Light behind the surface contributes nothing: only ambient (0.5 -> 128) remains.
    rt.setAmbientIntensity(0.5f);
    Scene behind = fixtures::sceneOf(fixtures::quad(0.f, 10.f), white);
    behind.addLight(Light(Vec3Df(0.f, 0.f, -10.f), Vec3Df(1.f, 1.f, 1.f), 1.f, 1.f));
    c = px(rt.render(behind, frontCamera(5.f), 16, 16), 8, 8);
    CHECK_CLOSE(c.r, 128, 1);

    // Light colour multiplies material colour; intensity and diffuse scale it.
    rt.setAmbientIntensity(0.f);
    Scene tinted = fixtures::sceneOf(fixtures::quad(0.f, 10.f), Material(0.5f, 0.f, Vec3Df(1.f, 0.5f, 0.f)));
    tinted.addLight(Light(Vec3Df(0.f, 0.f, 10.f), Vec3Df(0.f, 1.f, 1.f), 2.f, 1.f));
    c = px(rt.render(tinted, frontCamera(5.f), 16, 16), 8, 8);
    CHECK_EQ(c.r, 0);                 // material red * light red (0)
    CHECK_CLOSE(c.g, 128, 1);         // 0.5 (diffuse) * 2 (intensity) * 0.5 (material) * 1 (light)
    CHECK_EQ(c.b, 0);                 // material blue is 0
}

TEST_CASE("render: background colour, statistics and determinism") {
    Scene s = fixtures::sceneOf(fixtures::quad(0.f, 0.5f));
    RayTracer rt;
    rt.setDebugMode(RayTracer::DebugMode::AMBIENT);
    rt.setBackgroundColor(Vec3Df(0.2f, 0.4f, 0.6f));
    const Image a = rt.render(s, frontCamera(5.f), 24, 24);
    CHECK_EQ(px(a, 0, 0), (RGB{51, 102, 153}));
    CHECK_EQ(px(a, 12, 12), kWhite);
    const RayTracer::Stats& st = rt.getLastStats();
    CHECK_EQ(st.rays, 576ul);
    CHECK(st.hits > 0 && st.hits < 576);

    const Image b = rt.render(s, frontCamera(5.f), 24, 24);
    REQUIRE(a.sizeInBytes() == b.sizeInBytes());
    CHECK_MSG(std::memcmp(a.data(), b.data(), a.sizeInBytes()) == 0, "same input, same pixels");
}

TEST_CASE("render: object id differs between objects") {
    Scene s;
    s.addObject(Object(fixtures::quad(0.f, 0.3f), fixtures::white()));
    s.addObject(Object(fixtures::quad(-1.f, 10.f), fixtures::white()));
    RayTracer rt;
    rt.setDebugMode(RayTracer::DebugMode::OBJECT_ID);
    const Image img = rt.render(s, frontCamera(5.f), 32, 32);
    const RGB centre = px(img, 16, 16), corner = px(img, 0, 0);
    CHECK(centre != corner);
    CHECK(centre != kBlack);
    CHECK(corner != kBlack);
}

TEST_CASE("render: image rows are top-down") {
    // A quad only in the upper half of the view (y > 0) must light up the top rows of the image.
    const Vec3Df n(0.f, 0.f, 1.f);
    std::vector<Vertex> v = {Vertex(Vec3Df(-10.f, 0.5f, 0.f), n), Vertex(Vec3Df(10.f, 0.5f, 0.f), n),
                             Vertex(Vec3Df(10.f, 10.f, 0.f), n), Vertex(Vec3Df(-10.f, 10.f, 0.f), n)};
    Scene s = fixtures::sceneOf(Mesh(v, {Triangle(0, 1, 2), Triangle(0, 2, 3)}));
    RayTracer rt;
    rt.setDebugMode(RayTracer::DebugMode::HIT_MASK);
    const Image img = rt.render(s, frontCamera(5.f), 16, 16);
    CHECK_EQ(px(img, 8, 0), kWhite);    // top row
    CHECK_EQ(px(img, 8, 15), kBlack);   // bottom row
}
