#include <algorithm>
#include <chrono>
#include <cstdio>
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

bool samePixels(const Image& a, const Image& b) {
    return a.sizeInBytes() == b.sizeInBytes() && std::memcmp(a.data(), b.data(), a.sizeInBytes()) == 0;
}

// Pixels of tile (tx, ty) of a `tile`-sized grid are equal in a and b.
bool sameTile(const Image& a, const Image& b, int tx, int ty, int tile) {
    for (int y = ty * tile; y < std::min(a.height(), (ty + 1) * tile); ++y) {
        for (int x = tx * tile; x < std::min(a.width(), (tx + 1) * tile); ++x) {
            unsigned char r1, g1, b1, r2, g2, b2;
            a.getPixel(x, y, r1, g1, b1);
            b.getPixel(x, y, r2, g2, b2);
            if (r1 != r2 || g1 != g2 || b1 != b2) return false;
        }
    }
    return true;
}

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

TEST_CASE("renderjob: same pixels and statistics as RayTracer::render for any tile size and thread count") {
    const Scene scene = teapotScene();
    const Camera camera = frameOf(scene);
    RayTracer rt;
    const Image reference = rt.render(scene, camera, 64, 48);
    const RayTracer::Stats ref = rt.getLastStats();

    // 24 leaves partial tiles; 128 exceeds the image (one tile, so one worker).
    for (unsigned int tile : {1u, 16u, 24u, 128u})
    for (unsigned int threads : {1u, 2u, 3u, 8u}) {
        const std::string label = "tile size " + std::to_string(tile) + ", " + std::to_string(threads) + " threads";
        RenderJob job(rt, scene, camera, 64, 48, tile, Vec3Df(0.f, 0.f, 0.f), threads);
        CHECK_EQ(job.threadCount(), std::min(threads, job.totalTiles()));
        CHECK_CLOSE(job.progress(), 0.f, 0.f);
        CHECK_MSG(!job.isDone(), label);
        job.start();
        job.wait();
        CHECK_MSG(job.isDone() && job.isComplete() && !job.isCancelled(), label);
        CHECK_CLOSE(job.progress(), 1.f, 0.f);
        CHECK_EQ(job.completedTiles(), job.totalTiles());

        CHECK_MSG(samePixels(job.snapshot(), reference), label);

        const RayTracer::Stats st = job.stats();
        CHECK_EQ(st.rays, ref.rays);
        CHECK_EQ(st.hits, ref.hits);
        CHECK_CLOSE(st.minHitDist, ref.minHitDist, 0.f);
        CHECK_CLOSE(st.maxHitDist, ref.maxHitDist, 0.f);
        CHECK(st.seconds >= 0.0);
        CHECK(job.elapsedSeconds() >= st.seconds);
    }
}

TEST_CASE("renderjob: jittered anti-aliasing does not depend on tile order or threads") {
    // The jitter seed is per pixel, so tiles and threads must give the same
    // picture as the synchronous render.
    const Scene scene = teapotScene();
    const Camera camera = frameOf(scene);
    RayTracer rt;
    rt.setAntiAliasing(2, true);
    const Image reference = rt.render(scene, camera, 64, 48);
    RenderJob job(rt, scene, camera, 64, 48, 24, Vec3Df(0.f, 0.f, 0.f), 4);
    job.start();
    job.wait();
    const Image img = job.snapshot();
    REQUIRE(img.sizeInBytes() == reference.sizeInBytes());
    CHECK(std::memcmp(img.data(), reference.data(), img.sizeInBytes()) == 0);
    CHECK_EQ(job.stats().rays, rt.getLastStats().rays);
    CHECK_EQ(job.stats().rays, 64ul * 48ul * 4ul);
}

TEST_CASE("renderjob: soft shadows do not depend on tile order or threads") {
    // Shadow samples are seeded per pixel, like the jitter.
    Scene scene = teapotScene();
    scene.addGroundPlane();
    const Camera camera = frameOf(scene);
    RayTracer rt;
    rt.setShadowSamples(3);
    rt.setAntiAliasing(2, true);
    const Image reference = rt.render(scene, camera, 64, 48);
    RenderJob job(rt, scene, camera, 64, 48, 24, Vec3Df(0.f, 0.f, 0.f), 4);
    job.start();
    job.wait();
    const Image img = job.snapshot();
    REQUIRE(img.sizeInBytes() == reference.sizeInBytes());
    CHECK(std::memcmp(img.data(), reference.data(), img.sizeInBytes()) == 0);

    // The default lights have a radius, so the soft picture is not the hard one.
    rt.setShadowSamples(1);
    const Image hard = rt.render(scene, camera, 64, 48);
    CHECK(std::memcmp(hard.data(), reference.data(), hard.sizeInBytes()) != 0);
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
    // Heavy enough (soft shadows over a ground) that cancelling after the
    // first tile lands leaves most of the image untouched, even with threads.
    Scene scene = teapotScene();
    scene.addGroundPlane();
    const Camera camera = frameOf(scene);
    RayTracer rt;
    rt.setShadowSamples(6);
    rt.setAntiAliasing(2, true);
    const int kTile = 16;
    for (unsigned int threads : {1u, 4u}) {
        const std::string label = std::to_string(threads) + " threads";
        RenderJob job(rt, scene, camera, 256, 256, kTile, Vec3Df(0.5f, 0.5f, 0.5f), threads);  // 256 tiles
        job.start();
        // Let at least one tile finish, then stop.
        while (job.completedTiles() == 0 && !job.isDone()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        job.cancel();
        job.wait();
        CHECK_MSG(job.isDone() && job.isCancelled() && !job.isComplete(), label);
        CHECK_MSG(job.completedTiles() >= 1, label);
        CHECK_MSG(job.completedTiles() < job.totalTiles(), label);

        // Every tile is either final (equal to the reference render) or still
        // entirely the pending colour, and the final ones are the published ones.
        const Image img = job.snapshot();
        const Image reference = rt.render(scene, camera, 256, 256);
        Image pending(256, 256, Image::RGB888);
        pending.fill(128, 128, 128);
        unsigned int finalTiles = 0, pendingTiles = 0;
        for (int ty = 0; ty < 16; ++ty) {
            for (int tx = 0; tx < 16; ++tx) {
                if (sameTile(img, reference, tx, ty, kTile))
                    ++finalTiles;
                else if (sameTile(img, pending, tx, ty, kTile))
                    ++pendingTiles;
            }
        }
        CHECK_EQ(finalTiles + pendingTiles, 256u);
        CHECK_EQ(finalTiles, job.completedTiles());
        // Tiles are handed out top row first: the last one never started.
        CHECK_MSG(sameTile(img, pending, 15, 15, kTile), label);
        // Stats only cover published tiles.
        CHECK_EQ(job.stats().rays, static_cast<unsigned long>(job.completedTiles()) * kTile * kTile * 4ul);
    }
}

TEST_CASE("renderjob: thread count defaults to the cores and never exceeds the tiles") {
    const Scene scene = teapotScene();
    RayTracer rt;
    CHECK(RenderJob::defaultThreadCount() >= 1u);
    RenderJob automatic(rt, scene, frameOf(scene), 256, 256, 16);
    CHECK_EQ(automatic.threadCount(), std::min(RenderJob::defaultThreadCount(), 256u));
    RenderJob oneTile(rt, scene, frameOf(scene), 16, 16, 32, Vec3Df(0.f, 0.f, 0.f), 8);
    CHECK_EQ(oneTile.threadCount(), 1u);
    RenderJob four(rt, scene, frameOf(scene), 64, 64, 32, Vec3Df(0.f, 0.f, 0.f), 16);
    CHECK_EQ(four.threadCount(), 4u);
}

TEST_CASE("renderjob: threads speed up a sampled render") {
    // Reports the speedup; asserts only the pixels, since timing depends on
    // the machine (and CI runners may have two cores).
    Scene scene = teapotScene();
    scene.addGroundPlane();
    const Camera camera = frameOf(scene);
    RayTracer rt;
    rt.setShadowSamples(4);
    rt.setAntiAliasing(2, true);
    const unsigned int cores = RenderJob::defaultThreadCount();
    RenderJob one(rt, scene, camera, 128, 128, 16, Vec3Df(0.f, 0.f, 0.f), 1);
    one.start();
    one.wait();
    RenderJob all(rt, scene, camera, 128, 128, 16, Vec3Df(0.f, 0.f, 0.f), cores);
    all.start();
    all.wait();
    CHECK(samePixels(one.snapshot(), all.snapshot()));
    CHECK_EQ(one.stats().rays, all.stats().rays);
    CHECK_EQ(one.stats().hits, all.stats().hits);
    const double t1 = one.stats().seconds, tn = all.stats().seconds;
    std::printf("  [renderjob] 128x128 soft shadows: 1 thread %.3f s, %u threads %.3f s, speedup x%.1f\n", t1,
                all.threadCount(), tn, tn > 0.0 ? t1 / tn : 0.0);
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
