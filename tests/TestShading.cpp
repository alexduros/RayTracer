#include <cmath>
#include <cstring>

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
Vec3Df shadeAt(RayTracer& rt, const Scene& s, const Vec3Df& from, const Vec3Df& to) {
    Vec3Df d = to - from;
    d.normalize();
    RayTracer::Stats st;
    return rt.trace(s, Ray(from, d), st);
}

}  // namespace

TEST_CASE("shading: the ground plane sits under the model and leaves the framing box alone") {
    Scene s = fixtures::sceneOf(fixtures::cube(Vec3Df(1.f, 2.f, 3.f), 2.f));  // bottom at y = 1
    const BoundingBox before = s.getBoundingBox();
    s.addGroundPlane();
    REQUIRE(s.getObjects().size() == 2);
    CHECK(s.getObjects()[1].isBackdrop());
    CHECK(!s.getObjects()[0].isBackdrop());
    CHECK(s.getBoundingBox().getMin() == before.getMin());  // a backdrop never grows the box
    CHECK(s.getBoundingBox().getMax() == before.getMax());

    const Mesh& g = s.getObjects()[1].getMesh();
    CHECK_EQ(g.getTriangles().size(), 2u);
    for (const Vertex& v : g.getVertices()) {
        CHECK_CLOSE(v.getPos()[1], 1.f, 1e-6);  // at the bottom of the model
        CHECK(v.getNormal() == Vec3Df(0.f, 1.f, 0.f));
    }
    CHECK_CLOSE(g.getVertices()[0].getPos()[0], 1.f - 3.f * 2.f, 1e-5);  // extent 3 x size, centred on the model

    // Visible from above, culled from below (single-sided).
    RayTracer rt;
    RayTracer::Hit hit;
    CHECK(rt.closestHit(s, Ray(Vec3Df(5.f, 10.f, 3.f), Vec3Df(0.f, -1.f, 0.f)), hit));
    CHECK_EQ(hit.objectIndex, 1u);
    CHECK(!rt.closestHit(s, Ray(Vec3Df(5.f, -10.f, 3.f), Vec3Df(0.f, 1.f, 0.f)), hit));

    s.removeBackdrops();
    CHECK_EQ(s.getObjects().size(), 1u);
    CHECK(s.getBoundingBox().getMax() == before.getMax());
}

TEST_CASE("shading: a blocked light leaves only the ambient term") {
    // A wall at z = 0 facing +Z, a cube floating in front of it, one white
    // light far out on +Z. The wall point behind the cube is in its shadow.
    const Material white(1.f, 0.f, kWhiteColor);
    Scene s;
    s.addObject(Object(fixtures::quad(0.f, 10.f), white));
    s.addObject(Object(fixtures::cube(Vec3Df(0.f, 0.f, 2.f), 1.f), white));
    s.addLight(Light(Vec3Df(0.f, 0.f, 10.f), kWhiteColor, 1.f, 1.f));
    RayTracer rt;
    rt.setSpecularEnabled(false);
    rt.setAmbientIntensity(0.15f);

    const Vec3Df behindCube = shadeAt(rt, s, Vec3Df(0.f, 0.f, 1.f), Vec3Df(0.f, 0.f, 0.f));
    CHECK_CLOSE(behindCube[0], 0.15f, 1e-4);  // ambient only
    const Vec3Df open = shadeAt(rt, s, Vec3Df(3.f, 0.f, 1.f), Vec3Df(3.f, 0.f, 0.f));
    CHECK_CLOSE(open[0], 0.15f + 10.f / std::sqrt(109.f), 1e-3);  // ambient + n.l with l = (-3, 0, 10)

    rt.setShadows(false);
    CHECK_CLOSE(shadeAt(rt, s, Vec3Df(0.f, 0.f, 1.f), Vec3Df(0.f, 0.f, 0.f))[0], 1.15f, 1e-4);
}

TEST_CASE("shading: a surface never shadows itself") {
    // Oblique light on a lone plane: shadows on and off must give the same
    // picture, i.e. no acne from shadow rays re-hitting their own surface.
    Scene s = fixtures::sceneOf(fixtures::quad(0.f, 10.f), Material(1.f, 0.f, kWhiteColor));
    s.addLight(Light(Vec3Df(2.f, 3.f, 6.f), kWhiteColor, 1.f, 1.f));
    const Camera cam = Camera::lookAt(Vec3Df(0.f, 0.f, 5.f), Vec3Df(0.f, 0.f, 0.f), Vec3Df(0.f, 1.f, 0.f), kPi / 4.f, 1.f);
    RayTracer rt;
    rt.setSpecularEnabled(false);
    rt.setShadows(true);
    const Image a = rt.render(s, cam, 32, 32);
    rt.setShadows(false);
    const Image b = rt.render(s, cam, 32, 32);
    REQUIRE(a.sizeInBytes() == b.sizeInBytes());
    CHECK(std::memcmp(a.data(), b.data(), a.sizeInBytes()) == 0);
}

TEST_CASE("shading: Blinn-Phong highlight peaks on the half-vector and sharpens with shininess") {
    const Vec3Df eye(0.f, 0.f, 5.f);
    Scene s = fixtures::sceneOf(fixtures::quad(0.f, 10.f), Material(1.f, 0.5f, kWhiteColor, 32.f));
    s.addLight(Light(eye, kWhiteColor, 1.f, 1.f));  // light at the eye: half-vector = normal
    RayTracer rt;
    rt.setAmbientIntensity(0.f);
    CHECK_CLOSE(shadeAt(rt, s, eye, Vec3Df(0.f, 0.f, 0.f))[0], 1.f + 0.5f, 1e-4);  // diffuse 1 + specular 0.5
    rt.setSpecularEnabled(false);
    CHECK_CLOSE(shadeAt(rt, s, eye, Vec3Df(0.f, 0.f, 0.f))[0], 1.f, 1e-4);         // Lambert alone
    rt.setSpecularEnabled(true);

    // Light off axis at (5, 0, 5): n.l = 0.707, n.h = 0.924. The highlight is
    // small at shininess 32 and vanishes at 256; the diffuse part stays.
    s.getLights()[0].setPos(Vec3Df(5.f, 0.f, 5.f));
    const float soft = shadeAt(rt, s, eye, Vec3Df(0.f, 0.f, 0.f))[0];
    s.getObjects()[0].getMaterial().setShininess(256.f);
    const float sharp = shadeAt(rt, s, eye, Vec3Df(0.f, 0.f, 0.f))[0];
    const float diffuse = 1.f / std::sqrt(2.f);
    CHECK(soft > sharp);
    CHECK_CLOSE(sharp, diffuse, 1e-3);
    CHECK(soft > diffuse + 0.02f && soft < diffuse + 0.06f);  // 0.5 * 0.924^32 = 0.04

    // The highlight is white, whatever the material colour.
    s.getObjects()[0].getMaterial().setShininess(32.f);
    s.getObjects()[0].getMaterial().setColor(Vec3Df(1.f, 0.f, 0.f));
    const Vec3Df red = shadeAt(rt, s, eye, Vec3Df(0.f, 0.f, 0.f));
    CHECK(red[1] > 0.02f && red[1] < 0.06f);  // green channel: specular only
}

TEST_CASE("shading: the ground plane receives the model's shadow in a render") {
    // Cube on its ground plane, one white light high on +X: the plane just
    // past the cube on -X is in shadow, the plane far on +X is lit.
    Scene s = fixtures::sceneOf(fixtures::cube(Vec3Df(0.f, 0.5f, 0.f), 1.f), Material(1.f, 0.f, kWhiteColor));
    s.addGroundPlane(Material(1.f, 0.f, kWhiteColor));
    s.addLight(Light(Vec3Df(10.f, 6.f, 0.f), kWhiteColor, 1.f, 1.f));
    RayTracer rt;
    rt.setAmbientIntensity(0.1f);
    // Straight down onto the plane (half-size 3 x the cube's size = 3) at
    // x = -0.8, behind the cube as seen from the light, and at x = +2.5, on
    // the light's side of the cube, in the open.
    const Vec3Df shadowed = shadeAt(rt, s, Vec3Df(-0.8f, 5.f, 0.f), Vec3Df(-0.8f, 0.f, 0.f));
    const Vec3Df lit = shadeAt(rt, s, Vec3Df(2.5f, 5.f, 0.f), Vec3Df(2.5f, 0.f, 0.f));
    CHECK_CLOSE(shadowed[0], 0.1f, 1e-4);
    CHECK_CLOSE(lit[0], 0.1f + 6.f / std::sqrt(7.5f * 7.5f + 36.f), 1e-3);  // ambient + n.l, l = (7.5, 6, 0)
}
