#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "Camera.h"
#include "Fixtures.h"
#include "Image.h"
#include "Light.h"
#include "Material.h"
#include "Optics.h"
#include "RayTracer.h"
#include "Scene.h"
#include "Test.h"

namespace {

const float kPi = 3.14159265358979f;
const Vec3Df kWhiteColor(1.f, 1.f, 1.f);

float degrees(float d) { return d * kPi / 180.f; }

/// Unit direction in the xz plane, `theta` from straight down (-Z) toward +X.
Vec3Df downAt(float theta) { return Vec3Df(std::sin(theta), 0.f, -std::cos(theta)); }

/// Linear colour seen along a ray.
Vec3Df traceColor(const RayTracer& rt, const Scene& s, const Vec3Df& origin, const Vec3Df& direction) {
    RayTracer::Stats st;
    return rt.trace(s, Ray(origin, direction), st);
}

/// A rectangle [x0, x1] x [-10, 10] in the plane z = 0, facing +Z.
Mesh floorPanel(float x0, float x1) {
    const Vec3Df n(0.f, 0.f, 1.f);
    const std::vector<Vertex> v = {Vertex(Vec3Df(x0, -10.f, 0.f), n), Vertex(Vec3Df(x1, -10.f, 0.f), n),
                                   Vertex(Vec3Df(x1, 10.f, 0.f), n), Vertex(Vec3Df(x0, 10.f, 0.f), n)};
    return Mesh(v, {Triangle(0, 1, 2), Triangle(0, 2, 3)});
}

Material glass(float ior) {
    Material m(1.f, 0.f, kWhiteColor);
    m.setTransparency(1.f);
    m.setIor(ior);
    return m;
}

// A floor z = 0, red for x < 0 and green for x > 0, lit by a far white light
// (so nearly uniformly), and optionally a clear glass slab z in [1, 2] over
// it. Shadows are left off by the tests: glass casts opaque shadows here.
Scene slabScene(bool withSlab, float ior) {
    Scene s;
    s.addObject(Object(floorPanel(-10.f, 0.f), Material(1.f, 0.f, Vec3Df(1.f, 0.f, 0.f))));
    s.addObject(Object(floorPanel(0.f, 10.f), Material(1.f, 0.f, Vec3Df(0.f, 1.f, 0.f))));
    if (withSlab) s.addObject(Object(fixtures::box(Vec3Df(-6.f, -6.f, 1.f), Vec3Df(6.f, 6.f, 2.f)), glass(ior)));
    s.addLight(Light(Vec3Df(0.f, 0.f, 1000.f), kWhiteColor, 1.f, 0.f));
    return s;
}

/// Where the red/green edge appears to a ray leaving z = 5 at `theta`: the
/// first origin x, scanning upward in 0.001 steps, whose ray sees green.
float apparentEdge(const RayTracer& rt, const Scene& s, float theta) {
    const Vec3Df d = downAt(theta);
    for (int k = 0; k < 2000; ++k) {
        const float x = -3.5f + 0.001f * static_cast<float>(k);
        const Vec3Df c = traceColor(rt, s, Vec3Df(x, 0.f, 5.f), d);
        if (c[1] > c[0]) return x;
    }
    return 1e9f;
}

}  // namespace

TEST_CASE("optics: Snell's law bends the ray, and past the critical angle nothing gets through") {
    const Vec3Df up(0.f, 0.f, 1.f);
    for (float angle : {0.f, 10.f, 30.f, 60.f, 85.f}) {
        const Vec3Df d = downAt(degrees(angle));
        Vec3Df t;
        REQUIRE(optics::refract(d, up, 1.f / 1.5f, t));
        CHECK_CLOSE(t.getLength(), 1.f, 1e-5);
        CHECK_CLOSE(1.5f * t[0], d[0], 1e-5);  // n1 sin(i) = n2 sin(t), in the plane of incidence
        CHECK(t[2] < 0.f);                     // it goes on through the surface
        CHECK_CLOSE(t[1], 0.f, 1e-6);
        REQUIRE(optics::refract(d, up, 1.f, t));  // same index: straight on
        CHECK_CLOSE(t[0], d[0], 1e-6);
        CHECK_CLOSE(t[2], d[2], 1e-6);
    }
    // Leaving glass for air: the critical angle is asin(1 / 1.5) = 41.8 degrees.
    const float critical = std::asin(1.f / 1.5f);
    Vec3Df t;
    CHECK(optics::refract(downAt(critical - 0.01f), Vec3Df(0.f, 0.f, 1.f), 1.5f, t));
    CHECK(!optics::refract(downAt(critical + 0.01f), Vec3Df(0.f, 0.f, 1.f), 1.5f, t));

    const Vec3Df r = optics::reflect(downAt(degrees(30.f)), up);  // angle out = angle in
    CHECK_CLOSE(r[0], std::sin(degrees(30.f)), 1e-6);
    CHECK_CLOSE(r[2], std::cos(degrees(30.f)), 1e-6);
}

TEST_CASE("optics: the Fresnel equations give 4 % at normal incidence, all at grazing, none between equal indices") {
    CHECK_CLOSE(optics::fresnel(1.f, 1.f, 1.5f), 0.04f, 1e-6);  // ((1.5 - 1) / (1.5 + 1))^2
    CHECK_CLOSE(optics::fresnel(1.f, 1.5f, 1.f), 0.04f, 1e-6);  // the same from inside
    CHECK_CLOSE(optics::fresnel(0.f, 1.f, 1.5f), 1.f, 1e-6);    // grazing: a mirror
    float previous = 0.f;
    for (int k = 0; k <= 90; k += 5) {
        const float cosi = std::cos(degrees(static_cast<float>(k)));
        CHECK_CLOSE(optics::fresnel(cosi, 1.3f, 1.3f), 0.f, 1e-6);  // no interface, no reflection
        const float f = optics::fresnel(cosi, 1.f, 1.5f);
        CHECK_MSG(f >= previous - 1e-6f, "rises toward grazing, " + std::to_string(k) + " degrees");
        previous = f;
    }
    // Brewster's angle, tan(i) = n2 / n1: the p-polarised half vanishes and
    // only the s half reflects.
    const float brewster = std::atan(1.5f), ti = std::asin(std::sin(brewster) / 1.5f);
    const float rs = std::sin(brewster - ti) / std::sin(brewster + ti);
    CHECK_CLOSE(optics::fresnel(std::cos(brewster), 1.f, 1.5f), 0.5f * rs * rs, 1e-5);
    // Reciprocity: the same share reflects going in at i or coming out at t.
    for (float angle : {10.f, 25.f, 40.f}) {
        const float i = degrees(angle), t = std::asin(std::sin(i) / 1.5f);
        CHECK_CLOSE(optics::fresnel(std::cos(i), 1.f, 1.5f), optics::fresnel(std::cos(t), 1.5f, 1.f), 1e-5);
    }
    // Total internal reflection.
    CHECK_CLOSE(optics::fresnel(std::cos(degrees(45.f)), 1.5f, 1.f), 1.f, 0.f);
}

TEST_CASE("refraction: a slab of index 1 is invisible") {
    const Scene bare = slabScene(false, 1.f), slab = slabScene(true, 1.f);
    RayTracer rt;
    rt.setShadows(false);
    for (float angle : {0.f, 20.f, 40.f}) {
        for (float x : {-3.f, -1.2f, 0.9f, 2.5f}) {
            const Vec3Df o(x, 0.3f, 5.f), d = downAt(degrees(angle));
            const Vec3Df through = traceColor(rt, slab, o, d), direct = traceColor(rt, bare, o, d);
            const std::string at = std::to_string(angle) + " degrees, x = " + std::to_string(x);
            for (int c = 0; c < 3; ++c) CHECK_MSG(std::fabs(through[c] - direct[c]) < 1e-3f, at);
        }
    }
}

TEST_CASE("refraction: a slab of index n shifts what lies under it by thickness x (tan i - tan t)") {
    // Through parallel faces the ray leaves in its original direction, moved
    // sideways: the red/green edge appears where the tilted ray, bent inside
    // the unit-thick slab, lands on x = 0.
    RayTracer rt;
    rt.setShadows(false);
    const float theta = degrees(30.f);
    const float bare = apparentEdge(rt, slabScene(false, 1.f), theta);
    CHECK_CLOSE(bare, -5.f * std::tan(theta), 0.002f);  // the scan itself, no slab
    float previousShift = 0.f;
    for (float ior : {1.33f, 1.5f, 2.4f}) {  // water, glass, diamond
        const float t = std::asin(std::sin(theta) / ior);
        const float expected = std::tan(theta) - std::tan(t);
        const float shift = apparentEdge(rt, slabScene(true, ior), theta) - bare;
        CHECK_MSG(std::fabs(shift - expected) < 0.003f, "index " + std::to_string(ior) + ": shift " +
                                                             std::to_string(shift) + ", expected " +
                                                             std::to_string(expected));
        CHECK(shift > previousShift);
        previousShift = shift;
    }
}

TEST_CASE("refraction: through the slab comes the light the two faces let through, never more") {
    // Clear glass reflects F at each face (the Fresnel share) and passes the
    // rest: what reaches the eye through both faces is (1 - F_in)(1 - F_out)
    // of the floor behind, plus a trace of light bounced inside.
    RayTracer rt;
    rt.setShadows(false);
    const Scene bare = slabScene(false, 1.5f), slab = slabScene(true, 1.5f);
    for (float angle : {0.f, 30.f, 60.f}) {
        const float i = degrees(angle), t = std::asin(std::sin(i) / 1.5f);
        const float passed = (1.f - optics::fresnel(std::cos(i), 1.f, 1.5f)) *
                             (1.f - optics::fresnel(std::cos(t), 1.5f, 1.f));
        // From z = 5, aimed to land at x = -2.5 through the slab (3 tan i above
        // it, tan t inside, tan i below): well inside the red half either way.
        const Vec3Df o(-2.5f - 4.f * std::tan(i) - std::tan(t), 0.f, 5.f), d = downAt(i);
        const float through = traceColor(rt, slab, o, d)[0], direct = traceColor(rt, bare, o, d)[0];
        const std::string at = std::to_string(angle) + " degrees: " + std::to_string(through / direct) +
                               " of the light, expected " + std::to_string(passed);
        CHECK_MSG(through <= direct, at);
        CHECK_MSG(std::fabs(through / direct - passed) < 0.01f, at);
    }
}

TEST_CASE("refraction: an opaque material, or a depth of 0, gives the old picture bit for bit") {
    Scene s = slabScene(true, 1.5f);
    const Camera cam = Camera::lookAt(Vec3Df(2.f, -3.f, 6.f), Vec3Df(0.f, 0.f, 0.f), Vec3Df(0.f, 0.f, 1.f), kPi / 3.f, 1.f);
    RayTracer rt;
    const Image glassImage = rt.render(s, cam, 32, 32);
    rt.setMaxDepth(0);
    const Image depthZero = rt.render(s, cam, 32, 32);
    rt.setMaxDepth(4);
    s.getObjects()[2].getMaterial().setTransparency(0.f);
    const Image opaque = rt.render(s, cam, 32, 32);
    REQUIRE(opaque.sizeInBytes() == depthZero.sizeInBytes());
    CHECK(std::memcmp(opaque.data(), depthZero.data(), opaque.sizeInBytes()) == 0);
    CHECK(std::memcmp(opaque.data(), glassImage.data(), opaque.sizeInBytes()) != 0);
    CHECK_EQ(rt.getLastStats().rays, 32ul * 32ul);  // bent rays are not primary rays
}

TEST_CASE("refraction: materials clamp transparency to [0, 1] and the index to 1 or more") {
    Material m;
    CHECK_CLOSE(m.getTransparency(), 0.f, 0.f);
    CHECK_CLOSE(m.getIor(), 1.5f, 0.f);
    m.setTransparency(2.f);
    CHECK_CLOSE(m.getTransparency(), 1.f, 0.f);
    m.setTransparency(-1.f);
    CHECK_CLOSE(m.getTransparency(), 0.f, 0.f);
    m.setIor(0.5f);
    CHECK_CLOSE(m.getIor(), 1.f, 0.f);
}
