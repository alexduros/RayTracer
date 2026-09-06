// A raytrace running on a worker thread, tile by tile, that can be watched
// and cancelled while it runs.
//
// The job owns copies of the tracer settings, the scene and the camera, so
// the caller may keep editing its own while the render is in flight. Pixels
// are identical to RayTracer::render(): tiles only change the order of work,
// and a test asserts the byte-for-byte equality.
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
#include "Image.h"
#include "RayTracer.h"
#include "Scene.h"
#include "Vec3D.h"

class RenderJob {
public:
    /// `pendingColor` (linear [0,1]) fills the image until a tile lands.
    RenderJob (const RayTracer & tracer, const Scene & scene, const Camera & camera,
               unsigned int width, unsigned int height, unsigned int tileSize = 32,
               const Vec3Df & pendingColor = Vec3Df (0.f, 0.f, 0.f));
    ~RenderJob ();  // cancels and joins
    RenderJob (const RenderJob &) = delete;
    RenderJob & operator= (const RenderJob &) = delete;

    /// Spawns the worker. Calling it twice is harmless.
    void start ();
    /// Ask the worker to stop after the row it is tracing. Non-blocking.
    void cancel ();
    /// Block until the worker has exited (after completion or a cancel).
    void wait ();

    /// The worker has exited, either because every tile is done or after cancel().
    bool isDone () const { return finished.load (); }
    /// Every tile was rendered.
    bool isComplete () const { return complete.load (); }
    /// Stopped by cancel() before the last tile.
    bool isCancelled () const { return finished.load () && !complete.load (); }

    unsigned int totalTiles () const { return tilesX * tilesY; }
    unsigned int completedTiles () const { return tilesDone.load (); }
    /// completedTiles / totalTiles, in [0, 1].
    float progress () const;
    /// Wall time since start(), frozen when the worker exits; 0 before start().
    double elapsedSeconds () const;
    unsigned int width () const { return w; }
    unsigned int height () const { return h; }

    /// Copy of the image so far: completed tiles hold final pixels, the rest
    /// still hold the pending colour.
    Image snapshot () const;
    /// Statistics over the completed tiles (all of them once complete);
    /// `seconds` is set when the worker exits.
    RayTracer::Stats stats () const;

private:
    void run ();

    RayTracer tracer;
    Scene scene;
    Camera camera;
    unsigned int w, h, tile, tilesX, tilesY;

    Image working;                 // worker-private
    mutable std::mutex mutex;
    Image published;               // guarded by mutex
    RayTracer::Stats accumulated;  // guarded by mutex
    std::atomic<unsigned int> tilesDone{0};
    std::atomic<bool> started{false};
    std::atomic<bool> cancelRequested{false};
    std::atomic<bool> complete{false};
    std::atomic<bool> finished{false};
    std::chrono::steady_clock::time_point startTime, endTime;
    std::thread worker;
};

#endif // RENDERJOB_H
