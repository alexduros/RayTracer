#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "Camera.h"
#include "Fixtures.h"
#include "Image.h"
#include "RayTracer.h"
#include "RenderJob.h"
#include "Scene.h"
#include "Test.h"

namespace {

const float kPi = 3.14159265358979f;

int gray(const Image& img, int x, int y) {
    unsigned char r, g, b;
    img.getPixel(x, y, r, g, b);
    return r;
}

bool sameBytes(const Image& a, const Image& b) {
    return a.sizeInBytes() == b.sizeInBytes() && std::memcmp(a.data(), b.data(), a.sizeInBytes()) == 0;
}

/// Camera on +Z at distance z looking at the origin (so focused on the plane
/// z = 0), 45 degree vertical fov.
Camera frontCamera(float z, float aspect) {
    return Camera::lookAt(Vec3Df(0.f, 0.f, z), Vec3Df(0.f, 0.f, 0.f), Vec3Df(0.f, 1.f, 0.f), kPi / 4.f, aspect);
}

/// A rectangle in the plane z = 0 from x = -10 to x = edge, y in [-10, 10],
/// facing +Z: one vertical edge in view, the others far outside the frame.
Scene edgeScene(float edge) {
    const Vec3Df n(0.f, 0.f, 1.f);
    std::vector<Vertex> v = {Vertex(Vec3Df(-10.f, -10.f, 0.f), n), Vertex(Vec3Df(edge, -10.f, 0.f), n),
                             Vertex(Vec3Df(edge, 10.f, 0.f), n), Vertex(Vec3Df(-10.f, 10.f, 0.f), n)};
    std::vector<Triangle> t = {Triangle(0, 1, 2), Triangle(0, 2, 3)};
    return fixtures::sceneOf(Mesh(v, t));
}

/// Share of a disk of radius 1 lying left of a vertical line, its centre t
/// to the right of that line (t < 0: to its left).
float diskShareBehind(float t) {
    if (t >= 1.f) return 0.f;
    if (t <= -1.f) return 1.f;
    return (std::acos(t) - t * std::sqrt(1.f - t * t)) / kPi;
}

}  // namespace

TEST_CASE("dof: a camera is a pinhole focused on what it looks at") {
    const Camera c = frontCamera(5.f, 1.f);
    CHECK_CLOSE(c.aperture, 0.f, 0.f);
    CHECK_CLOSE(c.focusDistance, 5.f, 1e-6);
    const Camera orbit = Camera::orbit(Vec3Df(1.f, 2.f, 3.f), 7.f, 30.f, 20.f, kPi / 4.f, 1.5f);
    CHECK_CLOSE(orbit.focusDistance, 7.f, 1e-5);
    RayTracer rt;
    CHECK_EQ(rt.getLensSamplesPerAxis(), 4u);
}

TEST_CASE("dof: without an aperture the lens changes nothing, bit for bit") {
    Camera c = Camera::orbit(Vec3Df(0.f, 0.f, 0.f), 5.f, 30.f, 20.f, kPi / 4.f, 1.5f);
    const Ray pinhole = c.primaryRay(7, 3, 24, 16, 0.25f, 0.75f);
    const Ray lens = c.primaryRay(7, 3, 24, 16, 0.25f, 0.75f, 0.7f, -0.3f);
    for (int k = 0; k < 3; ++k) {
        CHECK_EQ(lens.getOrigin()[k], pinhole.getOrigin()[k]);
        CHECK_EQ(lens.getDirection()[k], pinhole.getDirection()[k]);
    }

    // The whole frame: lens samples are ignored, one ray per pixel as before.
    const Scene scene = fixtures::sceneOf(fixtures::cube(Vec3Df(0.f, 0.f, 0.f), 1.5f));
    RayTracer rt;
    rt.setAntiAliasing(2, true);
    rt.setLensSamples(1);
    const Image reference = rt.render(scene, c, 24, 16);
    rt.setLensSamples(8);
    const Image img = rt.render(scene, c, 24, 16);
    CHECK_MSG(sameBytes(img, reference), "aperture 0 is the pinhole picture");
    CHECK_EQ(rt.getLastStats().rays, 24ul * 16ul * 4ul);
}

TEST_CASE("dof: every ray of a pixel leaves the lens and crosses the pinhole ray on the plane in focus") {
    Camera c = Camera::orbit(Vec3Df(0.5f, -0.2f, 0.1f), 6.f, 35.f, 25.f, kPi / 3.f, 1.5f);
    c.aperture = 0.3f;
    c.focusDistance = 4.f;
    const unsigned int pixels[][2] = {{0, 0}, {11, 7}, {23, 15}, {5, 12}};
    for (const auto& px : pixels) {
        const Ray pinhole = c.primaryRay(px[0], px[1], 24, 16, 0.3f, 0.6f);
        const Vec3Df& d0 = pinhole.getDirection();
        const Vec3Df target = c.pos + d0 * (c.focusDistance / Vec3Df::dotProduct(d0, c.dir));
        for (int j = 0; j < 5; ++j) {
            for (int i = 0; i < 5; ++i) {
                float lx = 0.f, ly = 0.f;
                Sampler::concentricDisk((i + 0.5f) / 5.f, (j + 0.5f) / 5.f, lx, ly);
                const Ray r = c.primaryRay(px[0], px[1], 24, 16, 0.3f, 0.6f, lx, ly);
                // On the lens: in the plane through pos facing dir, within the aperture.
                const Vec3Df offset = r.getOrigin() - c.pos;
                CHECK_CLOSE(Vec3Df::dotProduct(offset, c.dir), 0.f, 1e-6);
                CHECK_CLOSE(offset.getLength(), c.aperture * std::sqrt(lx * lx + ly * ly), 1e-6);
                CHECK(offset.getLength() <= c.aperture + 1e-6f);
                // Through the point of the plane in focus the pinhole ray meets.
                const float t = c.focusDistance / Vec3Df::dotProduct(r.getDirection(), c.dir);
                const Vec3Df p = r.getOrigin() + r.getDirection() * t;
                CHECK_CLOSE((p - target).getLength(), 0.f, 1e-5);
            }
        }
    }
}

TEST_CASE("dof: what lies on the plane in focus stays sharp") {
    // Edge between two pixel centres, on the plane in focus (z = 0, 5 away):
    // all 64 lens rays of a sample meet the same point, so the picture is the
    // pinhole's to the byte, jittered anti-aliasing included (its stream is
    // not shifted by the lens's).
    const float p = 2.f * 5.f * std::tan(kPi / 8.f) / 16.f;  // pixel size at depth 5
    const Scene scene = edgeScene(0.3f * p);
    Camera camera = frontCamera(5.f, 1.f);
    RayTracer rt;
    rt.setDebugMode(RayTracer::DebugMode::HIT_MASK);
    rt.setLensSamples(8);
    for (unsigned int aa : {1u, 4u}) {
        rt.setAntiAliasing(aa, aa > 1);
        camera.aperture = 0.f;
        const Image pinhole = rt.render(scene, camera, 16, 16);
        camera.aperture = 0.5f;  // about two pixels: a wide blur on any other plane
        const Image lens = rt.render(scene, camera, 16, 16);
        CHECK_MSG(sameBytes(lens, pinhole), "aa " + std::to_string(aa));
        CHECK_EQ(rt.getLastStats().rays, 16ul * 16ul * aa * aa * 64ul);
        if (aa == 1) {
            for (int x = 0; x < 16; ++x) {
                const int g = gray(lens, x, 8);
                CHECK_MSG(g == 0 || g == 255, "column " + std::to_string(x) + ": " + std::to_string(g));
            }
        } else {
            const int edge = gray(lens, 8, 8);
            CHECK_MSG(edge > 0 && edge < 255, "the edge pixel stays partly covered, as without a lens");
        }
    }
}

TEST_CASE("dof: off the plane in focus an edge spreads over the circle of confusion") {
    // Camera 5 from the plane z = 0, lens radius A focused at f. A lens ray
    // from l on the lens to F on the plane in focus is at F z / f + l (1 - z /
    // f) at depth z, so on the quad (z = 5) the rays of a pixel cover a disk
    // of radius A |1 - 5 / f| around the pinhole ray's point (Potmesil &
    // Chakravarty's circle of confusion, here measured on the object). With
    // one ray per pixel and a vertical edge, a pixel's grey is the share of
    // that disk on the quad's side of the edge, uniform over the disk since
    // the concentric map keeps areas.
    const unsigned int width = 64, height = 16;
    const float p = 2.f * 5.f * std::tan(kPi / 8.f) / height;  // pixel size at depth 5
    const float edge = 0.3f * p;
    const Scene scene = edgeScene(edge);
    RayTracer rt;
    rt.setDebugMode(RayTracer::DebugMode::HIT_MASK);
    rt.setLensSamples(16);
    struct Case {
        float focus, aperture;
    };
    // Focused behind the quad, then in front of it, then twice the lens.
    for (const Case& k : {Case{2.5f, 3.2f * p}, Case{10.f, 3.2f * p}, Case{2.5f, 6.4f * p}}) {
        Camera camera = frontCamera(5.f, static_cast<float>(width) / height);
        camera.aperture = k.aperture;
        camera.focusDistance = k.focus;
        const float radius = k.aperture * std::fabs(1.f - 5.f / k.focus);  // world units on the quad
        const Image img = rt.render(scene, camera, width, height);
        int blurred = 0;
        for (unsigned int x = 0; x < width; ++x) {
            const float centre = (static_cast<float>(x) + 0.5f - width / 2.f) * p;  // pinhole point
            const float t = (centre - edge) / radius;
            const float expected = 255.f * diskShareBehind(t);
            const int g = gray(img, static_cast<int>(x), 8);
            const std::string label = "focus " + std::to_string(k.focus) + ", column " + std::to_string(x) + ": " +
                                      std::to_string(g) + " for " + std::to_string(expected);
            if (std::fabs(t) > 1.01f)
                CHECK_MSG(g == (t < 0.f ? 255 : 0), label);  // outside the disk: untouched
            else
                CHECK_MSG(std::fabs(g - expected) <= 8.f, label);
            if (g > 0 && g < 255) ++blurred;
        }
        // About the disk's diameter in pixels is grey.
        const float diameter = 2.f * radius / p;
        CHECK_MSG(std::fabs(blurred - diameter) <= 1.5f,
                  std::to_string(blurred) + " grey pixels for a disk of " + std::to_string(diameter));
    }
}

TEST_CASE("dof: lens rays are reproducible and do not depend on tiles or threads") {
    const Scene scene = fixtures::sceneOf(fixtures::cube(Vec3Df(0.f, 0.f, 0.f), 1.5f));
    Camera camera = Camera::orbit(Vec3Df(0.f, 0.f, 0.f), 4.f, 30.f, 20.f, kPi / 4.f, 1.5f);
    camera.aperture = 0.2f;
    camera.focusDistance = 2.5f;
    RayTracer rt;
    rt.setAntiAliasing(2, true);
    rt.setLensSamples(3);
    rt.setShadowSamples(2);
    const Image reference = rt.render(scene, camera, 48, 32);
    CHECK_EQ(rt.getLastStats().rays, 48ul * 32ul * 4ul * 9ul);
    CHECK_MSG(sameBytes(rt.render(scene, camera, 48, 32), reference), "same seeds, same picture");
    for (unsigned int threads : {1u, 3u}) {
        RenderJob job(rt, scene, camera, 48, 32, 7, Vec3Df(0.f, 0.f, 0.f), threads);
        job.start();
        job.wait();
        CHECK_MSG(sameBytes(job.snapshot(), reference), std::to_string(threads) + " threads");
        CHECK_EQ(job.stats().rays, rt.getLastStats().rays);
    }
    camera.aperture = 0.f;
    CHECK_MSG(!sameBytes(rt.render(scene, camera, 48, 32), reference), "the lens blurs the cube");
}
