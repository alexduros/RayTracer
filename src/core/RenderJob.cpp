#include "RenderJob.h"

#include <algorithm>
#include <cstring>
#include <vector>

namespace {

unsigned char toByte (float c) {
    const int v = static_cast<int> (c * 255.f + 0.5f);
    return static_cast<unsigned char> (std::max (0, std::min (255, v)));
}

} // namespace

RenderJob::RenderJob (const RayTracer & tracer, const Scene & scene, const Camera & camera,
                      unsigned int width, unsigned int height, unsigned int tileSize,
                      const Vec3Df & pendingColor, unsigned int threadCount)
    : tracer (tracer),
      scene (scene),
      camera (camera),
      w (width),
      h (height),
      tile (std::max (1u, tileSize)),
      tilesX ((width + tile - 1) / tile),
      tilesY ((height + tile - 1) / tile),
      threads (std::max (1u, std::min (threadCount ? threadCount : defaultThreadCount (), tilesX * tilesY))),
      working (width, height, Image::RGB888),
      published (width, height, Image::RGB888) {
    const unsigned char r = toByte (pendingColor[0]), g = toByte (pendingColor[1]), b = toByte (pendingColor[2]);
    working.fill (r, g, b);
    published.fill (r, g, b);
}

unsigned int RenderJob::defaultThreadCount () {
    return std::max (1u, std::thread::hardware_concurrency ());
}

RenderJob::~RenderJob () {
    cancel ();
    wait ();
}

void RenderJob::start () {
    if (started.exchange (true))
        return;
    startTime = std::chrono::steady_clock::now ();
    worker = std::thread (&RenderJob::run, this);
}

void RenderJob::cancel () {
    cancelRequested.store (true);
}

void RenderJob::wait () {
    if (worker.joinable ())
        worker.join ();
}

float RenderJob::progress () const {
    const unsigned int total = totalTiles ();
    return total ? static_cast<float> (tilesDone.load ()) / static_cast<float> (total) : 1.f;
}

double RenderJob::elapsedSeconds () const {
    if (!started.load ())
        return 0.0;
    const auto end = finished.load () ? endTime : std::chrono::steady_clock::now ();
    return std::chrono::duration<double> (end - startTime).count ();
}

Image RenderJob::snapshot () const {
    std::lock_guard<std::mutex> lock (mutex);
    return published;
}

RayTracer::Stats RenderJob::stats () const {
    std::lock_guard<std::mutex> lock (mutex);
    return accumulated;
}

void RenderJob::run () {
    std::vector<std::thread> helpers;
    helpers.reserve (threads - 1);
    for (unsigned int i = 1; i < threads; ++i)
        helpers.emplace_back (&RenderJob::work, this);
    work ();
    for (std::thread & t : helpers)
        t.join ();

    endTime = std::chrono::steady_clock::now ();
    {
        std::lock_guard<std::mutex> lock (mutex);
        accumulated.seconds = std::chrono::duration<double> (endTime - startTime).count ();
    }
    if (tilesDone.load () == totalTiles ())
        complete.store (true);
    finished.store (true);
}

void RenderJob::work () {
    const unsigned int total = totalTiles ();
    while (!cancelRequested.load ()) {
        const unsigned int index = nextTile.fetch_add (1);
        if (index >= total)
            return;
        const unsigned int x0 = (index % tilesX) * tile, y0 = (index / tilesX) * tile;
        const unsigned int x1 = std::min (w, x0 + tile), y1 = std::min (h, y0 + tile);

        // Trace the tile row by row into the shared working image (no other
        // worker touches these pixels) so a cancel lands within one row.
        RayTracer::Stats tileStats;
        for (unsigned int y = y0; y < y1; ++y) {
            if (cancelRequested.load ())
                return;
            tracer.renderRegion (scene, camera, w, h, x0, y, x1, y + 1, working, tileStats);
        }

        // Publish the finished tile: copy its rows and fold in its stats.
        {
            std::lock_guard<std::mutex> lock (mutex);
            const size_t rowBytes = static_cast<size_t> (x1 - x0) * 3;
            for (unsigned int y = y0; y < y1; ++y) {
                const size_t offset = (static_cast<size_t> (y) * w + x0) * 3;
                std::memcpy (published.data () + offset, working.data () + offset, rowBytes);
            }
            accumulated.accumulate (tileStats);
        }
        tilesDone.fetch_add (1);
    }
}
