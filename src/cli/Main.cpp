// ---------------------------------------------------------------------------
// raymini-cli: render an OFF model to a PNG without opening a window.
//
//   raymini-cli teapot --mode normals --size 512x512 --out renders/teapot.png
//
// Used by the golden-image tests, by CI and whenever a render is needed on a
// machine without a display.
// ---------------------------------------------------------------------------
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

#include "Camera.h"
#include "Image.h"
#include "RayTracer.h"
#include "RenderJob.h"
#include "Scene.h"

#ifndef RAYMINI_MODELS_DIR
#define RAYMINI_MODELS_DIR ""
#endif

namespace {

const char* kUsage = R"(Usage: raymini-cli <model.off|model.obj> [options]

A bare name such as "teapot" or "cube" resolves to the bundled models/ directory.
OBJ files load their MTL materials (one object per material).

Options:
  --out <file.png>       output path, parent directories are created
                         (default renders/<model>_<mode>.png)
  --size <W>x<H>         image size (default 256x256)
  --mode <mode>          lit | ambient | hitmask | normals | depth | objectid (default lit)
  --fov <deg>            vertical field of view (default 45)
  --yaw <deg>            orbit around the model about +Y (default 0: camera on +Z)
  --pitch <deg>          orbit elevation, -89..89 (default 0)
  --distance <factor>    camera distance = factor x model size (default 2)
  --depth <near> <far>   depth-mode range in world units (default: model extent)
  --quiet                only print errors
  -h, --help             this text
)";

struct Options {
    std::string model;
    std::string out;
    unsigned int width = 256, height = 256;
    RayTracer::DebugMode mode = RayTracer::DebugMode::LIT;
    std::string modeName = "lit";
    float fovDeg = 45.f;
    float yaw = 0.f, pitch = 0.f;
    float distance = 2.f;
    float depthNear = -1.f, depthFar = -1.f;  // < 0: automatic
    bool quiet = false;
};

bool parseMode(const std::string& name, RayTracer::DebugMode& mode) {
    using M = RayTracer::DebugMode;
    static const struct { const char* name; M mode; } table[] = {
        {"lit", M::LIT},         {"ambient", M::AMBIENT}, {"hitmask", M::HIT_MASK},
        {"normals", M::NORMALS}, {"depth", M::DEPTH},     {"objectid", M::OBJECT_ID},
    };
    for (const auto& e : table) {
        if (name == e.name) {
            mode = e.mode;
            return true;
        }
    }
    return false;
}

// Returns false on a usage error (the message has already been printed).
bool parseArgs(int argc, char** argv, Options& o) {
    auto value = [&](int& i, const char* flag) -> const char* {
        if (i + 1 >= argc) {
            std::cerr << "missing value for " << flag << "\n";
            return nullptr;
        }
        return argv[++i];
    };
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        const char* v = nullptr;
        if (a == "-h" || a == "--help") {
            std::cout << kUsage;
            std::exit(0);
        } else if (a == "--quiet") {
            o.quiet = true;
        } else if (a == "--out") {
            if (!(v = value(i, "--out"))) return false;
            o.out = v;
        } else if (a == "--size") {
            if (!(v = value(i, "--size"))) return false;
            if (std::sscanf(v, "%ux%u", &o.width, &o.height) != 2 || o.width == 0 || o.height == 0) {
                std::cerr << "bad --size '" << v << "', expected WxH\n";
                return false;
            }
        } else if (a == "--mode") {
            if (!(v = value(i, "--mode"))) return false;
            o.modeName = v;
            if (!parseMode(o.modeName, o.mode)) {
                std::cerr << "unknown mode '" << v << "'\n";
                return false;
            }
        } else if (a == "--fov") {
            if (!(v = value(i, "--fov"))) return false;
            o.fovDeg = static_cast<float>(std::atof(v));
        } else if (a == "--yaw") {
            if (!(v = value(i, "--yaw"))) return false;
            o.yaw = static_cast<float>(std::atof(v));
        } else if (a == "--pitch") {
            if (!(v = value(i, "--pitch"))) return false;
            o.pitch = static_cast<float>(std::atof(v));
        } else if (a == "--distance") {
            if (!(v = value(i, "--distance"))) return false;
            o.distance = static_cast<float>(std::atof(v));
        } else if (a == "--depth") {
            if (i + 2 >= argc) {
                std::cerr << "--depth needs <near> <far>\n";
                return false;
            }
            o.depthNear = static_cast<float>(std::atof(argv[++i]));
            o.depthFar = static_cast<float>(std::atof(argv[++i]));
        } else if (!a.empty() && a[0] == '-') {
            std::cerr << "unknown option '" << a << "'\n" << kUsage;
            return false;
        } else if (o.model.empty()) {
            o.model = a;
        } else {
            std::cerr << "unexpected argument '" << a << "'\n";
            return false;
        }
    }
    if (o.model.empty()) {
        std::cerr << kUsage;
        return false;
    }
    if (o.fovDeg <= 0.f || o.fovDeg >= 180.f) {
        std::cerr << "--fov must be in (0, 180)\n";
        return false;
    }
    return true;
}

// "teapot" or "teapot.off" -> bundled models directory, unless the path exists as given.
std::string resolveModel(const std::string& given) {
    namespace fs = std::filesystem;
    if (fs::exists(given)) return given;
    // Bare name: the bundled directory, as given and with each supported extension.
    const fs::path dir = RAYMINI_MODELS_DIR;
    for (const char* ext : {"", ".off", ".obj"}) {
        const fs::path candidate = dir / (given + ext);
        if (fs::exists(candidate)) return candidate.string();
    }
    return given;  // let the loader report the error with the name the user typed
}

}  // namespace

int main(int argc, char** argv) {
    Options o;
    if (!parseArgs(argc, argv, o)) return 2;

    const std::string path = resolveModel(o.model);
    if (o.out.empty()) {
        o.out = "renders/" + std::filesystem::path(path).stem().string() + "_" + o.modeName + ".png";
    }

    Scene scene;
    try {
        scene.addObjectsFromFile(path);
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
    scene.addDefaultLights();

    const BoundingBox& bbox = scene.getBoundingBox();
    const float size = bbox.getSize();
    const float camDistance = o.distance * size;
    const float fovRad = o.fovDeg * 3.14159265f / 180.f;
    const Camera camera = Camera::frame(bbox, fovRad, static_cast<float>(o.width) / o.height,
                                        o.distance, o.yaw, o.pitch);

    RayTracer rt;
    rt.setDebugMode(o.mode);
    if (o.depthNear < 0.f || o.depthFar < 0.f) {
        rt.setDepthRange(std::max(0.f, camDistance - 0.5f * size), camDistance + 0.5f * size);
    } else {
        rt.setDepthRange(o.depthNear, o.depthFar);
    }

    // Same tile-by-tile worker as the GUI; on a terminal, show a percentage.
    RenderJob job(rt, scene, camera, o.width, o.height, 32);
    const bool showProgress = !o.quiet && isatty(fileno(stderr));
    job.start();
    while (!job.isDone()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        if (showProgress) std::fprintf(stderr, "\rrendering %3.0f%%", 100.f * job.progress());
    }
    job.wait();
    if (showProgress) std::fprintf(stderr, "\r                \r");
    const Image image = job.snapshot();
    const RayTracer::Stats st = job.stats();

    const auto parent = std::filesystem::path(o.out).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent);
    if (!image.save(o.out)) {
        std::cerr << "could not write " << o.out << "\n";
        return 1;
    }

    if (!o.quiet) {
        size_t nv = 0, nt = 0;
        for (const Object& o : scene.getObjects()) {
            nv += o.getMesh().getVertices().size();
            nt += o.getMesh().getTriangles().size();
        }
        const Vec3Df c = bbox.getCenter();
        std::printf("model   %s: %zu object(s), %zu vertices, %zu triangles, size %.3f, centre (%.3f, %.3f, %.3f)\n",
                    path.c_str(), scene.getObjects().size(), nv, nt, size, c[0], c[1], c[2]);
        std::printf("camera  pos (%.3f, %.3f, %.3f) dir (%.3f, %.3f, %.3f) fov %.1f yaw %.1f pitch %.1f\n",
                    camera.pos[0], camera.pos[1], camera.pos[2], camera.dir[0], camera.dir[1], camera.dir[2],
                    o.fovDeg, o.yaw, o.pitch);
        std::printf("render  %ux%u %s in %.3fs: %lu/%lu rays hit (%.1f%%), hit distance [%.3f, %.3f]\n",
                    o.width, o.height, o.modeName.c_str(), st.seconds, st.hits, st.rays,
                    st.rays ? 100.0 * st.hits / st.rays : 0.0, st.minHitDist, st.maxHitDist);
        std::printf("wrote   %s\n", o.out.c_str());
    }
    return 0;
}
