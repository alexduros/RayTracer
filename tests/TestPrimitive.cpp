#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

#include "Camera.h"
#include "Fixtures.h"
#include "Light.h"
#include "Material.h"
#include "Primitive.h"
#include "RayTracer.h"
#include "Scene.h"
#include "Test.h"

namespace {

const float kPi = 3.14159265358979f;

Ray toward(const Vec3Df& from, const Vec3Df& at, bool twoSided = false) {
    Vec3Df d = at - from;
    d.normalize();
    return Ray(from, d, twoSided);
}

/// The nearest hit of a primitive, from an unbounded search.
bool hitOf(const Primitive& p, const Ray& ray, Vertex& hit, float& t) {
    t = std::numeric_limits<float>::max();
    return p.intersect(ray, hit, t);
}

/// A UV sphere of `rings` x `2 * rings` quads, smooth normals from the sphere
/// itself: the tessellation an analytic sphere replaces.
Mesh uvSphere(const Vec3Df& centre, float radius, unsigned int rings) {
    std::vector<Vertex> v;
    std::vector<Triangle> t;
    const unsigned int slices = 2 * rings;
    for (unsigned int i = 0; i <= rings; ++i) {
        const float phi = kPi * static_cast<float>(i) / static_cast<float>(rings);
        for (unsigned int j = 0; j <= slices; ++j) {
            const float theta = 2.f * kPi * static_cast<float>(j) / static_cast<float>(slices);
            const Vec3Df n(std::sin(phi) * std::cos(theta), std::cos(phi), std::sin(phi) * std::sin(theta));
            v.push_back(Vertex(centre + radius * n, n));
        }
    }
    const unsigned int row = slices + 1;
    for (unsigned int i = 0; i < rings; ++i)
        for (unsigned int j = 0; j < slices; ++j) {
            const unsigned int a = i * row + j, b = a + 1, c = a + row, d = c + 1;
            t.push_back(Triangle(a, b, c));  // wound CCW seen from outside
            t.push_back(Triangle(b, d, c));
        }
    return Mesh(v, t);
}

}  // namespace

TEST_CASE("primitive: a sphere is hit at the root of its quadratic, from any direction") {
    const Vec3Df centre(1.f, -2.f, 0.5f);
    const float radius = 1.5f;
    const Sphere sphere(centre, radius);
    Vertex hit;
    float t = 0.f;

    // Head-on: the distance is exactly the gap between the origin and the surface.
    REQUIRE(hitOf(sphere, Ray(centre + Vec3Df(0.f, 0.f, 6.f), Vec3Df(0.f, 0.f, -1.f)), hit, t));
    CHECK_CLOSE(t, 6.f - radius, 1e-5);
    CHECK_CLOSE((hit.getPos() - centre).getLength(), radius, 1e-5);

    // Oblique rays: -b - sqrt(b^2 - c), the smaller root of the quadratic.
    std::minstd_rand rng(7u);
    auto uniform = [&rng] { return static_cast<float>(rng() % 20001) / 10000.f - 1.f; };
    int tested = 0;
    for (int k = 0; k < 200; ++k) {
        // From a point around the sphere, aimed at its neighbourhood: most
        // rays hit, a few pass beside it, and both answers are checked.
        Vec3Df away(uniform(), uniform(), uniform());
        if (away.getSquaredLength() < 1e-4f) continue;
        away.normalize();
        const Vec3Df origin = centre + 5.f * away;
        const Vec3Df target = centre + 1.6f * radius * Vec3Df(uniform(), uniform(), uniform());
        Vec3Df direction = target - origin;
        if (direction.getSquaredLength() < 1e-4f) continue;
        direction.normalize();
        const Ray ray(origin, direction);
        const Vec3Df oc = origin - centre;
        const float b = Vec3Df::dotProduct(oc, direction), c = Vec3Df::dotProduct(oc, oc) - radius * radius;
        const float discriminant = b * b - c;
        const bool outside = c > 0.f;
        const bool expected = discriminant >= 0.f && outside && -b - std::sqrt(discriminant) > 0.f;
        const bool found = hitOf(sphere, ray, hit, t);
        CHECK_MSG(found == expected, "ray " + std::to_string(k));
        if (!found) continue;
        ++tested;
        CHECK_CLOSE(t, -b - std::sqrt(discriminant), 1e-4);
        CHECK_CLOSE((hit.getPos() - centre).getLength(), radius, 1e-4);
        // The normal is the radius direction, unit, and faces the ray.
        Vec3Df n = (hit.getPos() - centre) / radius;
        n.normalize();
        CHECK(std::fabs(Vec3Df::dotProduct(hit.getNormal(), n) - 1.f) < 1e-4f);
        CHECK(Vec3Df::dotProduct(hit.getNormal(), direction) < 0.f);
    }
    CHECK_MSG(tested > 50, "only " + std::to_string(tested) + " rays hit the sphere, too few to mean much");

    // A ray that misses, and one that passes tangent to the surface.
    CHECK(!hitOf(sphere, Ray(centre + Vec3Df(0.f, 0.f, 6.f), Vec3Df(0.f, 1.f, 0.f)), hit, t));
    REQUIRE(hitOf(sphere, Ray(centre + Vec3Df(radius, 0.f, 6.f), Vec3Df(0.f, 0.f, -1.f)), hit, t));
    CHECK_CLOSE(t, 6.f, 1e-3);  // grazes the equator
}

TEST_CASE("primitive: from inside, only a two-sided ray meets the sphere, and it is leaving") {
    const Sphere sphere(Vec3Df(0.f, 0.f, 0.f), 2.f);
    Vertex hit;
    float t = 0.f;
    const Vec3Df inside(0.5f, 0.f, 0.f), direction(1.f, 0.f, 0.f);
    CHECK_MSG(!hitOf(sphere, Ray(inside, direction), hit, t), "back face culled, like a triangle");
    REQUIRE(hitOf(sphere, Ray(inside, direction, true), hit, t));
    CHECK_CLOSE(t, 1.5f, 1e-5);
    CHECK(Vec3Df::dotProduct(hit.getNormal(), direction) > 0.f);  // the normal still points outward

    // The scene agrees: a two-sided ray reports it as a back face, the exit of the glass.
    Scene s;
    s.addObject(Object(std::make_shared<Sphere>(Vec3Df(0.f, 0.f, 0.f), 2.f), fixtures::white()));
    RayTracer rt;
    RayTracer::Hit h;
    REQUIRE(rt.closestHit(s, Ray(inside, direction, true), h));
    CHECK(h.backFace);
    REQUIRE(rt.closestHit(s, Ray(Vec3Df(0.f, 0.f, 8.f), Vec3Df(0.f, 0.f, -1.f), true), h));
    CHECK(!h.backFace);
}

TEST_CASE("primitive: the silhouette of a sphere is a circle, to the pixel") {
    // A sphere of radius r seen from distance d fills a cone of half-angle
    // asin(r / d): on the image plane that is a disc of exactly
    // tan(asin(r / d)) / tan(fov / 2) of the half-height, in pixels.
    const float radius = 1.f, distance = 6.f, fov = kPi / 4.f;
    const unsigned int size = 128;
    Scene s;
    s.addObject(Object(std::make_shared<Sphere>(Vec3Df(0.f, 0.f, 0.f), radius), fixtures::white()));
    const Camera cam = Camera::lookAt(Vec3Df(0.f, 0.f, distance), Vec3Df(0.f, 0.f, 0.f), Vec3Df(0.f, 1.f, 0.f), fov, 1.f);
    RayTracer rt;
    const float expected = static_cast<float>(size) / 2.f * std::tan(std::asin(radius / distance)) / std::tan(fov / 2.f);

    unsigned int wrong = 0, covered = 0;
    RayTracer::Hit hit;
    for (unsigned int y = 0; y < size; ++y)
        for (unsigned int x = 0; x < size; ++x) {
            const bool found = rt.closestHit(s, cam.primaryRay(x, y, size, size), hit);
            const float dx = static_cast<float>(x) + 0.5f - static_cast<float>(size) / 2.f;
            const float dy = static_cast<float>(y) + 0.5f - static_cast<float>(size) / 2.f;
            const float r = std::sqrt(dx * dx + dy * dy);
            if (found) ++covered;
            // Pixels within half a pixel of the edge may fall either way.
            if (std::fabs(r - expected) > 0.75f && found != (r < expected)) ++wrong;
        }
    CHECK_MSG(wrong == 0, std::to_string(wrong) + " pixels disagree with the analytic disc");
    CHECK_MSG(covered > 400, "the sphere covers " + std::to_string(covered) + " pixels");
}

TEST_CASE("primitive: a tessellated sphere converges to the analytic one") {
    // The point of an equation over triangles: the error falls with the
    // subdivision and never reaches 0, while the sphere costs one root.
    const Vec3Df centre(0.f, 0.f, 0.f);
    const float radius = 1.f;
    const Sphere sphere(centre, radius);
    std::minstd_rand rng(11u);
    auto uniform = [&rng] { return static_cast<float>(rng() % 20001) / 10000.f - 1.f; };
    std::vector<Ray> rays;
    while (rays.size() < 300) {
        Vec3Df direction(uniform(), uniform(), uniform());
        if (direction.getSquaredLength() < 1e-4f) continue;
        direction.normalize();
        rays.push_back(Ray(centre - 4.f * direction, direction));  // aimed at the centre
    }

    float previous = 1.f;
    for (unsigned int rings : {4u, 8u, 16u, 32u}) {
        const Mesh mesh = uvSphere(centre, radius, rings);
        Scene meshScene;
        meshScene.addObject(Object(mesh, fixtures::white()));
        RayTracer rt;
        float worst = 0.f;
        for (const Ray& ray : rays) {
            Vertex exact;
            float t = 0.f;
            REQUIRE(hitOf(sphere, ray, exact, t));
            RayTracer::Hit approximate;
            REQUIRE(rt.closestHit(meshScene, ray, approximate));
            worst = std::max(worst, std::fabs(approximate.distance - t));
        }
        // A facet dips below the sphere by at most the sagitta of the angle
        // it spans, r (1 - cos(angle)): the two directions of a quad add up,
        // so the bound uses the full step, pi / rings.
        const float sagitta = radius * (1.f - std::cos(kPi / static_cast<float>(rings)));
        const std::string at = std::to_string(rings) + " rings: worst error " + std::to_string(worst);
        CHECK_MSG(worst < sagitta, at + ", over the sagitta " + std::to_string(sagitta));
        CHECK_MSG(worst > 0.f, at + ", a mesh never lands exactly on the sphere");
        CHECK_MSG(worst < 0.4f * previous, at + ", expected under " + std::to_string(0.4f * previous));
        std::printf("  [primitive] %2u rings: worst error %.6f (sagitta %.6f)\n", rings, worst, sagitta);
        previous = worst;
    }
    CHECK_MSG(previous < 3e-3f, "32 rings still off by " + std::to_string(previous));
}

TEST_CASE("primitive: a cylinder keeps its ends, a disc keeps its rim") {
    const Cylinder cylinder(Vec3Df(0.f, 0.f, 0.f), Vec3Df(0.f, 1.f, 0.f), 1.f, 3.f);
    Vertex hit;
    float t = 0.f;
    // Across the side, halfway up.
    REQUIRE(hitOf(cylinder, Ray(Vec3Df(5.f, 1.5f, 0.f), Vec3Df(-1.f, 0.f, 0.f)), hit, t));
    CHECK_CLOSE(t, 4.f, 1e-5);
    CHECK_CLOSE(hit.getPos()[1], 1.5f, 1e-5);
    CHECK_CLOSE(Vec3Df::dotProduct(hit.getNormal(), Vec3Df(0.f, 1.f, 0.f)), 0.f, 1e-6);  // perpendicular to the axis
    // Above the top and below the base: the side ends there.
    CHECK(!hitOf(cylinder, Ray(Vec3Df(5.f, 3.5f, 0.f), Vec3Df(-1.f, 0.f, 0.f)), hit, t));
    CHECK(!hitOf(cylinder, Ray(Vec3Df(5.f, -0.5f, 0.f), Vec3Df(-1.f, 0.f, 0.f)), hit, t));
    // Down the axis: it meets nothing, the tube is open.
    CHECK(!hitOf(cylinder, Ray(Vec3Df(0.f, 5.f, 0.f), Vec3Df(0.f, -1.f, 0.f)), hit, t));
    CHECK(cylinder.boundingBox().getMin()[1] <= 0.f && cylinder.boundingBox().getMax()[1] >= 3.f);

    const Disc disc(Vec3Df(0.f, 3.f, 0.f), Vec3Df(0.f, 1.f, 0.f), 1.f);
    REQUIRE(hitOf(disc, Ray(Vec3Df(0.5f, 8.f, 0.f), Vec3Df(0.f, -1.f, 0.f)), hit, t));
    CHECK_CLOSE(t, 5.f, 1e-5);
    CHECK(hit.getNormal() == Vec3Df(0.f, 1.f, 0.f));
    CHECK_MSG(!hitOf(disc, Ray(Vec3Df(1.5f, 8.f, 0.f), Vec3Df(0.f, -1.f, 0.f)), hit, t), "outside the rim");
    CHECK_MSG(!hitOf(disc, Ray(Vec3Df(0.f, 8.f, 0.f), Vec3Df(1.f, 0.f, 0.f)), hit, t), "parallel to the plane");
}

TEST_CASE("primitive: in a scene it shadows, reflects and refracts like a mesh") {
    // A sphere over the ground: it hides the plane behind it, casts its
    // shadow, and a mirror sphere shows the floor.
    Scene s;
    s.addObject(Object(fixtures::quad(0.f, 20.f), Material(1.f, 0.f, Vec3Df(1.f, 1.f, 1.f))));  // wall at z = 0
    s.addObject(Object(std::make_shared<Sphere>(Vec3Df(0.f, 0.f, 3.f), 1.f), Material(1.f, 0.f, Vec3Df(1.f, 0.f, 0.f))));
    s.addLight(Light(Vec3Df(0.f, 0.f, 20.f), Vec3Df(1.f, 1.f, 1.f), 1.f, 0.f));
    RayTracer rt;
    rt.setAmbientIntensity(0.1f);

    RayTracer::Hit hit;
    REQUIRE(rt.closestHit(s, Ray(Vec3Df(0.f, 0.f, 8.f), Vec3Df(0.f, 0.f, -1.f)), hit));
    CHECK_EQ(hit.objectIndex, 1u);  // the sphere, not the wall behind it
    CHECK_CLOSE(hit.distance, 4.f, 1e-5);

    RayTracer::Stats stats;
    const Vec3Df shadowed = rt.trace(s, toward(Vec3Df(0.f, 0.f, 1.f), Vec3Df(0.f, 0.f, 0.f)), stats);
    CHECK_CLOSE(shadowed[0], 0.1f, 1e-4);  // the wall behind the sphere: ambient only
    const Vec3Df lit = rt.trace(s, toward(Vec3Df(4.f, 0.f, 1.f), Vec3Df(4.f, 0.f, 0.f)), stats);
    CHECK(lit[0] > 0.9f);

    // Glass: a two-sided ray goes in and comes out, so the wall shows through.
    s.getObjects()[1].getMaterial().setTransparency(1.f);
    s.getObjects()[1].getMaterial().setIor(1.0f);  // index 1: the sphere disappears
    const Vec3Df through = rt.trace(s, Ray(Vec3Df(0.2f, 0.f, 8.f), Vec3Df(0.f, 0.f, -1.f)), stats);
    const Vec3Df direct = rt.trace(s, Ray(Vec3Df(0.2f, 0.f, 2.f), Vec3Df(0.f, 0.f, -1.f)), stats);
    for (int c = 0; c < 3; ++c) CHECK_CLOSE(through[c], direct[c], 2e-3);
}
