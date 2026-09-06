#include <chrono>
#include <cstring>
#include <string>
#include <thread>

#include "Camera.h"
#include "Image.h"
#include "RayTracer.h"
#include "RenderJob.h"
#include "Scene.h"
#include "Test.h"

namespace {

const float kPi = 3.14159265358979f;

Scene teapotScene() {
    Scene s;
    s.addObjectsFromFile(test::modelPath("teapot"));
    s.addDefaultLights();
    return s;
}

Camera frameOf(const Scene& s) { return Camera::frame(s.getBoundingBox(), kPi / 4.f, 1.f, 2.f, 25.f, 20.f); }

}  // namespace

TEST_CASE("renderjob: stats accumulate across tiles") {
    RayTracer::Stats a;
    a.rays = 10;
    a.hits = 2;
    a.minHitDist = 1.f;
    a.maxHitDist = 3.f;

    RayTracer::Stats none;  // a tile with no hits must not disturb min/max
    none.rays = 5;
    a.accumulate(none);
    CHECK_EQ(a.rays, 15ul);
    CHECK_EQ(a.hits, 2ul);
    CHECK_CLOSE(a.minHitDist, 1.f, 0.f);
    CHECK_CLOSE(a.maxHitDist, 3.f, 0.f);

    RayTracer::Stats b;
    b.rays = 1;
    b.hits = 1;
    b.minHitDist = 0.5f;
    b.maxHitDist = 0.5f;
    a.accumulate(b);
    CHECK_EQ(a.hits, 3ul);
    CHECK_CLOSE(a.minHitDist, 0.5f, 0.f);
    CHECK_CLOSE(a.maxHitDist, 3.f, 0.f);

    RayTracer::Stats empty;  // the first tile with hits defines min and max
    empty.accumulate(b);
    CHECK_CLOSE(empty.minHitDist, 0.5f, 0.f);
    CHECK_CLOSE(empty.maxHitDist, 0.5f, 0.f);
}

TEST_CASE("renderjob: same pixels and statistics as RayTracer::render for any tile size") {
    const Scene scene = teapotScene();
    const Camera camera = frameOf(scene);
    RayTracer rt;
    const Image reference = rt.render(scene, camera, 64, 48);
    const RayTracer::Stats ref = rt.getLastStats();

    for (unsigned int tile : {1u, 16u, 24u, 128u}) {  // 24 leaves partial tiles; 128 exceeds the image
        const std::string label = "tile size " + std::to_string(tile);
        RenderJob job(rt, scene, camera, 64, 48, tile);
        CHECK_CLOSE(job.progress(), 0.f, 0.f);
        CHECK_MSG(!job.isDone(), label);
        job.start();
        job.wait();
        CHECK_MSG(job.isDone() && job.isComplete() && !job.isCancelled(), label);
        CHECK_CLOSE(job.progress(), 1.f, 0.f);
        CHECK_EQ(job.completedTiles(), job.totalTiles());

        const Image img = job.snapshot();
        REQUIRE(img.sizeInBytes() == reference.sizeInBytes());
        CHECK_MSG(std::memcmp(img.data(), reference.data(), img.sizeInBytes()) == 0, label);

        const RayTracer::Stats st = job.stats();
        CHECK_EQ(st.rays, ref.rays);
        CHECK_EQ(st.hits, ref.hits);
        CHECK_CLOSE(st.minHitDist, ref.minHitDist, 0.f);
        CHECK_CLOSE(st.maxHitDist, ref.maxHitDist, 0.f);
        CHECK(st.seconds >= 0.0);
        CHECK(job.elapsedSeconds() >= st.seconds);
    }
}

TEST_CASE("renderjob: jittered anti-aliasing does not depend on tile order") {
    // The jitter seed is per pixel, so tiles (and later threads) must give the
    // same picture as the synchronous render.
    const Scene scene = teapotScene();
    const Camera camera = frameOf(scene);
    RayTracer rt;
    rt.setAntiAliasing(2, true);
    const Image reference = rt.render(scene, camera, 64, 48);
    RenderJob job(rt, scene, camera, 64, 48, 24);
    job.start();
    job.wait();
    const Image img = job.snapshot();
    REQUIRE(img.sizeInBytes() == reference.sizeInBytes());
    CHECK(std::memcmp(img.data(), reference.data(), img.sizeInBytes()) == 0);
    CHECK_EQ(job.stats().rays, rt.getLastStats().rays);
    CHECK_EQ(job.stats().rays, 64ul * 48ul * 4ul);
}

TEST_CASE("renderjob: pending colour fills the image until tiles land") {
    const Scene scene = teapotScene();
    RayTracer rt;
    RenderJob job(rt, scene, frameOf(scene), 8, 8, 8, Vec3Df(0.2f, 0.4f, 0.6f));
    CHECK_EQ(job.totalTiles(), 1u);
    CHECK_CLOSE(job.elapsedSeconds(), 0.0, 0.0);
    unsigned char r, g, b;
    job.snapshot().getPixel(3, 3, r, g, b);
    CHECK_EQ(int(r), 51);
    CHECK_EQ(int(g), 102);
    CHECK_EQ(int(b), 153);
}

TEST_CASE("renderjob: cancel stops early and keeps the finished tiles") {
    const Scene scene = teapotScene();
    const Camera camera = frameOf(scene);
    RayTracer rt;
    RenderJob job(rt, scene, camera, 256, 256, 16, Vec3Df(0.5f, 0.5f, 0.5f));  // 256 tiles
    job.start();
    // Let at least one tile finish, then stop.
    while (job.completedTiles() == 0 && !job.isDone()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    job.cancel();
    job.wait();
    CHECK(job.isDone());
    CHECK(job.isCancelled());
    CHECK(!job.isComplete());
    CHECK(job.completedTiles() >= 1);
    CHECK(job.completedTiles() < job.totalTiles());

    // The first tile (top-left 16x16) is final and equals the reference render.
    const Image img = job.snapshot();
    const Image reference = rt.render(scene, camera, 256, 256);
    bool firstTileMatches = true;
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            unsigned char r1, g1, b1, r2, g2, b2;
            img.getPixel(x, y, r1, g1, b1);
            reference.getPixel(x, y, r2, g2, b2);
            firstTileMatches = firstTileMatches && r1 == r2 && g1 == g2 && b1 == b2;
        }
    }
    CHECK(firstTileMatches);
    // The last tile never started: still the pending colour.
    unsigned char r, g, b;
    img.getPixel(255, 255, r, g, b);
    CHECK_EQ(int(r), 128);
    CHECK_EQ(int(g), 128);
    CHECK_EQ(int(b), 128);
    // Stats only cover published tiles.
    CHECK_EQ(job.stats().rays, static_cast<unsigned long>(job.completedTiles()) * 16ul * 16ul);
}

TEST_CASE("renderjob: destroying a running job joins its worker") {
    const Scene scene = teapotScene();
    RayTracer rt;
    {
        RenderJob job(rt, scene, frameOf(scene), 256, 256, 16);
        job.start();
        // Leaves scope while the worker is busy: the destructor must cancel and join.
    }
    CHECK(true);
}
