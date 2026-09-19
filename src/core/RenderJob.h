// A raytrace running on worker threads, tile by tile, that can be watched
// and cancelled while it runs.
//
// The job owns copies of the tracer settings, the scene and the camera, so
// the caller may keep editing its own while the render is in flight. Workers
// pull tile indices from a shared atomic counter (row by row, top first) and
// each keeps private statistics for the tile it traces, folded in when the
// tile is published. Pixels are identical to RayTracer::render() whatever
// the tile size and thread count: tiles and threads only change the order
// of work, and tests assert the byte-for-byte equality.
//
//   RenderJob job (tracer, scene, camera, 512, 384);
//   job.start ();
//   while (!job.isDone ()) { show (job.snapshot (), job.progress ()); ... }
//   Image final = job.snapshot ();
#ifndef RENDERJOB_H
#define RENDERJOB_H

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

#include "Camera.h"
#include "HdrImage.h"
#include "Image.h"
#include "RayTracer.h"
#include "Scene.h"
#include "Vec3D.h"

class RenderJob {
public:
    /// `pendingColor` (linear [0,1]) fills the image until a tile lands.
    /// `threads` workers trace in parallel; 0 means defaultThreadCount (). No
    /// more workers than tiles are started.
    RenderJob (const RayTracer & tracer, const Scene & scene, const Camera & camera,
               unsigned int width, unsigned int height, unsigned int tileSize = 32,
               const Vec3Df & pendingColor = Vec3Df (0.f, 0.f, 0.f), unsigned int threads = 0);
    ~RenderJob ();  // cancels and joins
    RenderJob (const RenderJob &) = delete;
    RenderJob & operator= (const RenderJob &) = delete;

    /// std::thread::hardware_concurrency (), or 1 when it is unknown.
    static unsigned int defaultThreadCount ();

    /// Spawns the workers. Calling it twice is harmless.
    void start ();
    /// Ask the workers to stop after the row each is tracing; tiles they had
    /// not finished stay unpublished. Non-blocking.
    void cancel ();
    /// Block until every worker has exited (after completion or a cancel).
    void wait ();

    /// The workers have exited, either because every tile is done or after cancel().
    bool isDone () const { return finished.load (); }
    /// Every tile was rendered.
    bool isComplete () const { return complete.load (); }
    /// Stopped by cancel() before the last tile.
    bool isCancelled () const { return finished.load () && !complete.load (); }

    unsigned int totalTiles () const { return tilesX * tilesY; }
    unsigned int completedTiles () const { return tilesDone.load (); }
    /// completedTiles / totalTiles, in [0, 1].
    float progress () const;
    /// Wall time since start(), frozen when the workers exit; 0 before start().
    double elapsedSeconds () const;
    /// Worker threads this job runs.
    unsigned int threadCount () const { return threads; }
    unsigned int width () const { return w; }
    unsigned int height () const { return h; }

    /// The image so far through the tracer's display: completed tiles hold
    /// final pixels, the rest still hold the pending colour.
    Image snapshot () const;
    /// The same in linear radiance, before the display.
    HdrImage hdrSnapshot () const;
    /// Statistics over the completed tiles (all of them once complete);
    /// `seconds` is set when the workers exit.
    RayTracer::Stats stats () const;

private:
    void run ();         // starts the other workers, works, joins them
    void work ();        // one worker: pull tiles until none are left or cancelled

    RayTracer tracer;
    Scene scene;
    Camera camera;
    unsigned int w, h, tile, tilesX, tilesY, threads;

    HdrImage working;              // each worker writes only the pixels of its own tile
    mutable std::mutex mutex;
    HdrImage published;            // guarded by mutex
    RayTracer::Stats accumulated;  // guarded by mutex
    std::atomic<unsigned int> nextTile{0};
    std::atomic<unsigned int> tilesDone{0};
    std::atomic<bool> started{false};
    std::atomic<bool> cancelRequested{false};
    std::atomic<bool> complete{false};
    std::atomic<bool> finished{false};
    std::chrono::steady_clock::time_point startTime, endTime;
    std::thread worker;            // runs run(), and so is also the first worker
};

#endif // RENDERJOB_H
