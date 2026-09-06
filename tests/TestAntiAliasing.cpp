#include <cmath>
#include <cstring>

#include "Camera.h"
#include "Fixtures.h"
#include "Image.h"
#include "RayTracer.h"
#include "Scene.h"
#include "Test.h"

namespace {

const float kPi = 3.14159265358979f;

int gray(const Image& img, int x, int y) {
    unsigned char r, g, b;
    img.getPixel(x, y, r, g, b);
    return r;
}

/// Camera on +Z at height z looking at the origin, 45 degree vertical fov, square.
Camera frontCamera(float z) {
    return Camera::lookAt(Vec3Df(0.f, 0.f, z), Vec3Df(0.f, 0.f, 0.f), Vec3Df(0.f, 1.f, 0.f), kPi / 4.f, 1.f);
}

// A quad whose right edge falls a quarter of the way into pixel column 12 of a
// 16-pixel-wide frame seen from z = 5: pixel (12, 8) is 25% covered.
Scene quarterCoveredScene(float& pixelSize) {
    pixelSize = 2.f * 5.f * std::tan(kPi / 8.f) / 16.f;
    return fixtures::sceneOf(fixtures::quad(0.f, 4.25f * pixelSize));
}

}  // namespace

TEST_CASE("aa: one ray per pixel is the default and keeps binary edges") {
    float p;
    const Scene scene = quarterCoveredScene(p);
    RayTracer rt;
    CHECK_EQ(rt.getAntiAliasingSamplesPerAxis(), 1u);
    CHECK(!rt.getAntiAliasingJitter());
    rt.setDebugMode(RayTracer::DebugMode::HIT_MASK);
    const Image img = rt.render(scene, frontCamera(5.f), 16, 16);
    CHECK_EQ(gray(img, 9, 8), 255);   // inside
    CHECK_EQ(gray(img, 12, 8), 0);    // centre sample at 4.5 px falls outside the edge at 4.25 px
    CHECK_EQ(rt.getLastStats().rays, 256ul);
}

TEST_CASE("aa: a regular grid turns a partly covered pixel grey in proportion to coverage") {
    float p;
    const Scene scene = quarterCoveredScene(p);
    RayTracer rt;
    rt.setDebugMode(RayTracer::DebugMode::HIT_MASK);
    rt.setAntiAliasing(4, false);  // 16 rays per pixel
    const Image img = rt.render(scene, frontCamera(5.f), 16, 16);
    CHECK_EQ(gray(img, 9, 8), 255);   // interior pixels are unchanged
    CHECK_EQ(gray(img, 0, 0), 0);     // so are empty ones
    CHECK_EQ(gray(img, 12, 8), 64);   // 4 of 16 sub-samples hit -> 25% -> 64
    CHECK_EQ(rt.getLastStats().rays, 256ul * 16ul);
    // Statistics count sub-samples: the hit ratio is the covered area.
    const RayTracer::Stats& st = rt.getLastStats();
    CHECK(st.hits > 0 && st.hits < st.rays);
}

TEST_CASE("aa: jitter is reproducible and differs from the regular grid") {
    // Edge 0.3 of the way into pixel column 12. The regular 4x4 grid samples
    // at 0.125, 0.375, ... so it sees exactly one column of cells (25%);
    // jittered rays in the second column (0.25..0.5) land on either side.
    // (An edge at exactly 0.25 would sit on a cell boundary and jitter could
    // not change any outcome.)
    const float p = 2.f * 5.f * std::tan(kPi / 8.f) / 16.f;
    const Scene scene = fixtures::sceneOf(fixtures::quad(0.f, 4.3f * p));
    RayTracer rt;
    rt.setDebugMode(RayTracer::DebugMode::HIT_MASK);
    rt.setAntiAliasing(4, true);
    const Image a = rt.render(scene, frontCamera(5.f), 16, 16);
    const Image b = rt.render(scene, frontCamera(5.f), 16, 16);
    REQUIRE(a.sizeInBytes() == b.sizeInBytes());
    CHECK_MSG(std::memcmp(a.data(), b.data(), a.sizeInBytes()) == 0, "same seed, same picture");
    CHECK_EQ(gray(a, 9, 8), 255);  // interior unaffected by jitter
    const int edge = gray(a, 12, 8);
    CHECK_MSG(edge > 0 && edge < 255, "edge pixel is a partial grey, got " + std::to_string(edge));

    rt.setAntiAliasing(4, false);
    const Image regular = rt.render(scene, frontCamera(5.f), 16, 16);
    CHECK_EQ(gray(regular, 12, 8), 64);  // the grid sees one cell column: 4 of 16
    CHECK_MSG(std::memcmp(a.data(), regular.data(), a.sizeInBytes()) != 0, "jittered and regular differ");
}

TEST_CASE("aa: supersampling averages in linear colour") {
    // Half the pixel covered by a white quad on a black background: the
    // average of the linear values is 0.5 -> 128, not a gamma-encoded grey.
    const float p = 2.f * 5.f * std::tan(kPi / 8.f) / 16.f;
    const Scene scene = fixtures::sceneOf(fixtures::quad(0.f, 4.5f * p));  // edge halfway into pixel 12
    RayTracer rt;
    rt.setDebugMode(RayTracer::DebugMode::HIT_MASK);
    rt.setAntiAliasing(4, false);
    const Image img = rt.render(scene, frontCamera(5.f), 16, 16);
    CHECK_EQ(gray(img, 12, 8), 128);
}
