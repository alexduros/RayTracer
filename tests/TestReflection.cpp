#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "Camera.h"
#include "Fixtures.h"
#include "Image.h"
#include "Light.h"
#include "Material.h"
#include "RayTracer.h"
#include "Scene.h"
#include "Test.h"

namespace {

const float kPi = 3.14159265358979f;
const Vec3Df kWhiteColor(1.f, 1.f, 1.f);

/// Linear colour of the surface seen along the ray from `from` to `to`.
Vec3Df shadeAt(const RayTracer& rt, const Scene& s, const Vec3Df& from, const Vec3Df& to) {
    Vec3Df d = to - from;
    d.normalize();
    RayTracer::Stats st;
    return rt.trace(s, Ray(from, d), st);
}

/// A square of half-size `half` in the plane z = c.z, centred on c, facing +Z
/// (front up) or -Z.
Mesh panel(const Vec3Df& c, float half, bool facingUp) {
    const Vec3Df n(0.f, 0.f, facingUp ? 1.f : -1.f);
    const std::vector<Vertex> v = {Vertex(c + Vec3Df(-half, -half, 0.f), n), Vertex(c + Vec3Df(half, -half, 0.f), n),
                                   Vertex(c + Vec3Df(half, half, 0.f), n), Vertex(c + Vec3Df(-half, half, 0.f), n)};
    const std::vector<Triangle> t = facingUp ? std::vector<Triangle>{Triangle(0, 1, 2), Triangle(0, 2, 3)}
                                             : std::vector<Triangle>{Triangle(0, 2, 1), Triangle(0, 3, 2)};
    return Mesh(v, t);
}

void checkColor(const Vec3Df& actual, const Vec3Df& expected, float tolerance, const std::string& what) {
    for (int c = 0; c < 3; ++c)
        CHECK_MSG(std::fabs(actual[c] - expected[c]) <= tolerance,
                  what + ", channel " + std::to_string(c) + ": " + std::to_string(actual[c]) + " instead of " +
                      std::to_string(expected[c]));
}

// A grey mirror z = 0 facing +Z, a small red panel above it at z = 4 around
// x = 4, facing down, one white light between them. From the eye at
// (-2, 0, 2) the ray to the origin leaves the mirror along (1, 0, 1) and
// meets the panel's centre, angle out = angle in.
const Vec3Df kEye(-2.f, 0.f, 2.f), kMirrorPoint(0.f, 0.f, 0.f), kPanelCentre(4.f, 0.f, 4.f);
const Vec3Df kGrey(0.3f, 0.3f, 0.3f), kRed(1.f, 0.f, 0.f);

Scene mirrorScene(float reflectivity) {
    Scene s;
    s.addObject(Object(panel(Vec3Df(0.f, 0.f, 0.f), 10.f, true), Material(1.f, 0.f, kGrey, 32.f, reflectivity)));
    s.addObject(Object(panel(kPanelCentre, 0.5f, false), Material(1.f, 0.f, kRed)));
    s.addLight(Light(Vec3Df(2.f, 3.f, 2.f), kWhiteColor, 1.f, 0.f));
    return s;
}

}  // namespace

TEST_CASE("reflection: a mirror shows the surface its reflected ray meets") {
    RayTracer rt;
    rt.setAmbientIntensity(0.1f);
    const Scene mirror = mirrorScene(1.f), matte = mirrorScene(0.f);
    // The panel's colour seen straight from below: Lambert depends only on
    // the point and its normal, so any ray reaching its centre sees this.
    const Vec3Df red = shadeAt(rt, matte, Vec3Df(4.f, 0.f, 1.f), kPanelCentre);
    REQUIRE(red[0] > 0.3f && red[1] == 0.f && red[2] == 0.f);
    const Vec3Df own = shadeAt(rt, matte, kEye, kMirrorPoint);
    REQUIRE(own[0] > 0.1f && own[0] == own[1] && own[1] == own[2]);

    // The reflected ray starts a hair off the mirror (1e-4 x the scene's
    // size, like a shadow ray), so it meets the panel that far from its
    // centre, where the light falls very slightly differently.
    const float offCentre = 1e-3f;
    checkColor(shadeAt(rt, mirror, kEye, kMirrorPoint), red, offCentre, "perfect mirror");
    // Elsewhere on the mirror the reflected ray escapes: the background.
    rt.setBackgroundColor(Vec3Df(0.1f, 0.2f, 0.3f));
    checkColor(shadeAt(rt, mirror, kEye, Vec3Df(-4.f, 0.f, 0.f)), Vec3Df(0.1f, 0.2f, 0.3f), 0.f,
               "reflection of nothing");
    rt.setBackgroundColor(Vec3Df(0.f, 0.f, 0.f));

    // A partial mirror blends its own shading with the reflection.
    const Scene quarter = mirrorScene(0.25f);
    checkColor(shadeAt(rt, quarter, kEye, kMirrorPoint), 0.75f * own + 0.25f * red, offCentre, "reflectivity 0.25");

    // Depth 0: the mirror is left with its own shading, exactly.
    rt.setMaxDepth(0);
    checkColor(shadeAt(rt, mirror, kEye, kMirrorPoint), own, 0.f, "max depth 0");
}

TEST_CASE("reflection: two facing mirrors stay between their colours and converge as the depth grows") {
    // Mirror A (z = 0, facing up) and mirror B (z = 2, facing down), both of
    // reflectivity k. A ray bouncing straight between them sees, at depth D,
    // c(D) = sum_{i<D} (1 - k) k^i L_i + k^D L_D with L alternating A, B: a
    // weighted average of the two local colours, never brighter than the
    // brighter one, closing in on (L_A + k L_B) / (1 + k) by k^D.
    const float k = 0.8f;
    Scene s;
    s.addObject(Object(panel(Vec3Df(0.f, 0.f, 0.f), 10.f, true), Material(1.f, 0.f, Vec3Df(0.9f, 0.5f, 0.1f), 32.f, k)));
    s.addObject(Object(panel(Vec3Df(0.f, 0.f, 2.f), 10.f, false), Material(1.f, 0.f, Vec3Df(0.1f, 0.5f, 0.9f), 32.f, k)));
    s.addLight(Light(Vec3Df(1.f, 1.f, 1.f), kWhiteColor, 0.8f, 0.f));
    RayTracer rt;
    const Vec3Df middle(0.f, 0.f, 1.f), onA(0.f, 0.f, 0.f), onB(0.f, 0.f, 2.f);

    rt.setMaxDepth(0);
    const Vec3Df la = shadeAt(rt, s, middle, onA), lb = shadeAt(rt, s, middle, onB);
    for (int c = 0; c < 3; ++c) REQUIRE(la[c] <= 1.f && lb[c] <= 1.f);

    Vec3Df previous = la;
    for (unsigned int depth = 1; depth <= 40; ++depth) {
        rt.setMaxDepth(depth);
        const Vec3Df now = shadeAt(rt, s, middle, onA);
        const std::string at = "depth " + std::to_string(depth);
        for (int c = 0; c < 3; ++c) {
            CHECK_MSG(now[c] <= std::max(la[c], lb[c]) + 1e-5f, at + ": brighter than both mirrors");
            CHECK_MSG(now[c] >= std::min(la[c], lb[c]) - 1e-5f, at + ": darker than both mirrors");
            const float step = std::pow(k, static_cast<float>(depth)) * std::fabs(la[c] - lb[c]);
            CHECK_MSG(std::fabs(std::fabs(now[c] - previous[c]) - step) < 1e-5f,
                      at + ": the last bounce should move the colour by k^depth |L_A - L_B|");
        }
        previous = now;
    }
    checkColor(previous, (la + k * lb) / (1.f + k), 1e-3f, "the infinite hall of mirrors");
}

TEST_CASE("reflection: a mirror floor shows the model standing on it") {
    // A red cube on a white ground, one white light high in front. From the
    // eye at (0, 0.5, 3) the ground point (0, 0, 1.5) reflects the ray to the
    // cube's front face; the point (2.5, 0, 1.5) reflects it into the void.
    Scene s = fixtures::sceneOf(fixtures::cube(Vec3Df(0.f, 0.5f, 0.f), 1.f), Material(1.f, 0.f, kRed));
    s.addGroundPlane(Material(1.f, 0.f, kWhiteColor));
    s.addLight(Light(Vec3Df(0.f, 4.f, 4.f), kWhiteColor, 1.f, 0.f));
    const Vec3Df eye(0.f, 0.5f, 3.f), underCube(0.f, 0.f, 1.5f), aside(2.5f, 0.f, 1.5f);
    RayTracer rt;

    const Vec3Df matteUnder = shadeAt(rt, s, eye, underCube);
    CHECK_CLOSE(matteUnder[0], matteUnder[1], 0.f);  // white floor, white light: grey
    s.setGroundReflectivity(0.5f);
    CHECK_CLOSE(s.getObjects()[0].getMaterial().getReflectivity(), 0.f, 0.f);  // the model keeps its own
    const Vec3Df mirrorUnder = shadeAt(rt, s, eye, underCube);
    CHECK_MSG(mirrorUnder[0] > mirrorUnder[1] + 0.1f, "the red cube shows in the floor");
    CHECK_CLOSE(mirrorUnder[1], 0.5f * matteUnder[1], 1e-5);  // half the floor + a reflection with no green
    const Vec3Df mirrorAside = shadeAt(rt, s, eye, aside);
    CHECK_CLOSE(mirrorAside[0], mirrorAside[1], 0.f);  // nothing reflected there but the black background

    // A whole render: reflections change it, a depth of 0 restores the matte one bit for bit.
    const Camera cam = Camera::lookAt(eye, Vec3Df(0.f, 0.3f, 0.f), Vec3Df(0.f, 1.f, 0.f), kPi / 4.f, 1.f);
    const Image mirrored = rt.render(s, cam, 32, 32);
    // Reflected rays are not primary rays: the statistics count the pixels' rays only.
    CHECK_EQ(rt.getLastStats().rays, 32ul * 32ul);
    rt.setMaxDepth(0);
    const Image depthZero = rt.render(s, cam, 32, 32);
    s.setGroundReflectivity(0.f);
    rt.setMaxDepth(4);
    const Image matte = rt.render(s, cam, 32, 32);
    REQUIRE(matte.sizeInBytes() == depthZero.sizeInBytes());
    CHECK(std::memcmp(matte.data(), depthZero.data(), matte.sizeInBytes()) == 0);
    CHECK(std::memcmp(matte.data(), mirrored.data(), matte.sizeInBytes()) != 0);
}

TEST_CASE("reflection: materials clamp their reflectivity to [0, 1]") {
    Material m;
    CHECK_CLOSE(m.getReflectivity(), 0.f, 0.f);
    m.setReflectivity(1.5f);
    CHECK_CLOSE(m.getReflectivity(), 1.f, 0.f);
    m.setReflectivity(-0.5f);
    CHECK_CLOSE(m.getReflectivity(), 0.f, 0.f);
    CHECK_CLOSE(Material(1.f, 0.f, kWhiteColor, 32.f, 2.f).getReflectivity(), 1.f, 0.f);
}
