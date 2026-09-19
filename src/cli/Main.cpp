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
#include <optional>
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
#include "Orientation.h"
#include "RayTracer.h"
#include "RenderJob.h"
#include "Scene.h"

#ifndef RAYMINI_MODELS_DIR
#define RAYMINI_MODELS_DIR ""
#endif

namespace {

void printUsage(std::ostream& out) {
    out << R"(Usage: raymini-cli <model.off|model.obj> [options]

A bare name such as "teapot" or "cube" resolves to the bundled models/ directory.
OBJ files load their MTL materials (one object per material).

Options:
  --out <file.png>       output path, parent directories are created
                         (default renders/<model>_<mode>.png)
  --size <W>x<H>         image size (default 256x256)
  --mode <mode>          one of the modes below (default lit)
  --aa <n>               anti-aliasing: n x n rays per pixel, 1..8 (default 1 = off)
  --jitter               jitter the anti-aliasing rays inside their cells
  --up <axis>            which file axis points up: auto (orientation.txt next to the
                         model, else the flattest side is the bottom), +y, +z, -z, +x, -x, -y
  --ground               add a ground plane under the model (receives its shadows)
  --no-shadows           lit mode without shadow rays
  --shadow-samples <n>   soft shadows: n x n shadow rays per light over its disk, 1..16
                         (default 1 = hard shadows)
  --light-radius <f>     radius of every light as a fraction of the model size (default 0.1);
                         0 makes point lights, whose shadows stay hard
  --no-specular          lit mode without the Blinn-Phong highlight
  --ao <n>               ambient occlusion: n x n rays per hit over the hemisphere, 0..16,
                         in lit and ao modes (default 0 = off; 8 with --mode ao)
  --ao-radius <f>        how far occlusion looks, as a fraction of the model size (default 0.2)
  --reflectivity <k>     mirror share of the model's materials, 0..1 (default 0 = matte)
  --ground-reflectivity <k>
                         mirror share of the ground plane, 0..1 (implies --ground)
  --max-depth <n>        reflections followed per ray, 0..16 (default 4; 0 = none)
  --no-bvh               test every triangle (brute force) instead of the BVH, to compare
                         timings; the picture is identical
  --threads <n>          worker threads, 0 = one per core (default 0); the picture is identical
  --fov <deg>            vertical field of view (default 45)
  --yaw <deg>            orbit around the model about +Y (default 0: camera on +Z)
  --pitch <deg>          orbit elevation, -89..89 (default 0)
  --distance <factor>    camera distance = factor x model size (default 2)
  --depth <near> <far>   depth-mode range in world units (default: model extent)
  --quiet                only print errors
  -h, --help             this text

Modes (what each one computes, how to read it, and where it comes from):
)";
    for (int i = 0; i < RayTracer::kModeCount; ++i) {
        const RayTracer::ModeInfo& m = RayTracer::info(static_cast<RayTracer::DebugMode>(i));
        out << "  " << m.slug << "  " << m.name << "\n"
            << "      " << m.principle << "\n"
            << "      Read: " << m.reading << "\n"
            << "      Ref:  " << m.reference << "\n";
    }
    const RayTracer::ModeInfo& aa = RayTracer::antiAliasingInfo();
    out << "\n--aa / --jitter  " << aa.name << "\n"
        << "      " << aa.principle << "\n"
        << "      Read: " << aa.reading << "\n"
        << "      Ref:  " << aa.reference << "\n";
    const RayTracer::ModeInfo& soft = RayTracer::softShadowsInfo();
    out << "\n--shadow-samples / --light-radius  " << soft.name << "\n"
        << "      " << soft.principle << "\n"
        << "      Read: " << soft.reading << "\n"
        << "      Ref:  " << soft.reference << "\n";
    const RayTracer::ModeInfo& mirror = RayTracer::reflectionsInfo();
    out << "\n--reflectivity / --ground-reflectivity / --max-depth  " << mirror.name << "\n"
        << "      " << mirror.principle << "\n"
        << "      Read: " << mirror.reading << "\n"
        << "      Ref:  " << mirror.reference << "\n";
    out << "\nPlanned modes, not available yet (map: claudedocs/RENDERING_ROADMAP.md):\n";
    for (int i = 0; i < RayTracer::kPlannedModeCount; ++i) {
        const RayTracer::ModeInfo& p = RayTracer::plannedMode(i);
        out << "  " << p.name << "\n"
            << "      " << p.principle << "\n"
            << "      " << p.reading << "\n"
            << "      Ref:  " << p.reference << "\n";
    }
}

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
    unsigned int aa = 1;
    bool jitter = false;
    bool ground = false;
    bool shadows = true;
    unsigned int shadowSamples = 1;
    float lightRadius = -1.f;  // < 0: the default rig's
    bool specular = true;
    int aoSamples = -1;          // per axis, 0 = off; < 0: 8 in ao mode, else off
    float aoRadius = 0.2f;       // fraction of the model size
    float reflectivity = 0.f;        // model objects
    float groundReflectivity = 0.f;  // the ground plane
    unsigned int maxDepth = 4;
    bool bvh = true;
    unsigned int threads = 0;  // 0 = one per core
    std::optional<UpAxis> up;  // empty = auto
    bool quiet = false;
};

bool parseMode(const std::string& name, RayTracer::DebugMode& mode) {
    for (int i = 0; i < RayTracer::kModeCount; ++i) {
        const RayTracer::DebugMode m = static_cast<RayTracer::DebugMode>(i);
        if (name == RayTracer::info(m).slug) {
            mode = m;
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
            printUsage(std::cout);
            std::exit(0);
        } else if (a == "--quiet") {
            o.quiet = true;
        } else if (a == "--up") {
            if (!(v = value(i, "--up"))) return false;
            if (std::string(v) != "auto") {
                o.up = parseUpAxis(v);
                if (!o.up) {
                    std::cerr << "--up must be auto, +x, -x, +y, -y, +z or -z\n";
                    return false;
                }
            }
        } else if (a == "--ground") {
            o.ground = true;
        } else if (a == "--no-shadows") {
            o.shadows = false;
        } else if (a == "--shadow-samples") {
            if (!(v = value(i, "--shadow-samples"))) return false;
            const int n = std::atoi(v);
            if (n < 1 || n > 16) {
                std::cerr << "--shadow-samples must be between 1 and 16\n";
                return false;
            }
            o.shadowSamples = static_cast<unsigned int>(n);
        } else if (a == "--light-radius") {
            if (!(v = value(i, "--light-radius"))) return false;
            o.lightRadius = static_cast<float>(std::atof(v));
            if (o.lightRadius < 0.f) {
                std::cerr << "--light-radius must be 0 or more\n";
                return false;
            }
        } else if (a == "--no-specular") {
            o.specular = false;
        } else if (a == "--ao") {
            if (!(v = value(i, "--ao"))) return false;
            const int n = std::atoi(v);
            if (n < 0 || n > 16 || (n == 0 && std::string(v) != "0")) {
                std::cerr << "--ao must be between 0 (off) and 16\n";
                return false;
            }
            o.aoSamples = n;
        } else if (a == "--ao-radius") {
            if (!(v = value(i, "--ao-radius"))) return false;
            o.aoRadius = static_cast<float>(std::atof(v));
            if (o.aoRadius <= 0.f) {
                std::cerr << "--ao-radius must be more than 0\n";
                return false;
            }
        } else if (a == "--reflectivity" || a == "--ground-reflectivity") {
            if (!(v = value(i, a.c_str()))) return false;
            const float k = static_cast<float>(std::atof(v));
            if (k < 0.f || k > 1.f) {
                std::cerr << a << " must be between 0 and 1\n";
                return false;
            }
            if (a == "--reflectivity") {
                o.reflectivity = k;
            } else {
                o.groundReflectivity = k;
                o.ground = true;
            }
        } else if (a == "--max-depth") {
            if (!(v = value(i, "--max-depth"))) return false;
            const int n = std::atoi(v);
            if (n < 0 || n > 16 || (n == 0 && std::string(v) != "0")) {
                std::cerr << "--max-depth must be between 0 and 16\n";
                return false;
            }
            o.maxDepth = static_cast<unsigned int>(n);
        } else if (a == "--no-bvh") {
            o.bvh = false;
        } else if (a == "--threads") {
            if (!(v = value(i, "--threads"))) return false;
            const int n = std::atoi(v);
            if (n < 0 || n > 256 || (n == 0 && std::string(v) != "0")) {
                std::cerr << "--threads must be between 0 (one per core) and 256\n";
                return false;
            }
            o.threads = static_cast<unsigned int>(n);
        } else if (a == "--jitter") {
            o.jitter = true;
        } else if (a == "--aa") {
            if (!(v = value(i, "--aa"))) return false;
            const int n = std::atoi(v);
            if (n < 1 || n > 8) {
                std::cerr << "--aa must be between 1 and 8\n";
                return false;
            }
            o.aa = static_cast<unsigned int>(n);
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
            std::cerr << "unknown option '" << a << "'\n";
            printUsage(std::cerr);
            return false;
        } else if (o.model.empty()) {
            o.model = a;
        } else {
            std::cerr << "unexpected argument '" << a << "'\n";
            return false;
        }
    }
    if (o.model.empty()) {
        printUsage(std::cerr);
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
    // Orient before placing lights: the scene is Y-up, the file may not be.
    std::string upSource = "option";
    const UpAxis up = o.up ? *o.up : resolveUpAxis(path, scene, &upSource);
    scene.setUpAxis(up);
    scene.addDefaultLights();
    if (o.lightRadius >= 0.f) scene.setLightRadius(o.lightRadius);
    if (o.ground) scene.addGroundPlane();  // a backdrop: framing below still follows the model
    scene.setModelReflectivity(o.reflectivity);
    scene.setGroundReflectivity(o.groundReflectivity);

    const BoundingBox& bbox = scene.getBoundingBox();
    const float size = bbox.getSize();
    const float camDistance = o.distance * size;
    const float fovRad = o.fovDeg * 3.14159265f / 180.f;
    const Camera camera = Camera::frame(bbox, fovRad, static_cast<float>(o.width) / o.height,
                                        o.distance, o.yaw, o.pitch);

    RayTracer rt;
    rt.setDebugMode(o.mode);
    rt.setAntiAliasing(o.aa, o.jitter);
    rt.setShadows(o.shadows);
    rt.setShadowSamples(o.shadowSamples);
    rt.setSpecularEnabled(o.specular);
    rt.setMaxDepth(o.maxDepth);
    // The ao mode with no --ao would be all white: give it the occlusion it shows.
    if (o.aoSamples < 0) o.aoSamples = o.mode == RayTracer::DebugMode::AMBIENT_OCCLUSION ? 8 : 0;
    rt.setAmbientOcclusion(static_cast<unsigned int>(o.aoSamples), o.aoRadius * size);
    rt.setBvhEnabled(o.bvh);
    if (o.depthNear < 0.f || o.depthFar < 0.f) {
        rt.setDepthRange(std::max(0.f, camDistance - 0.5f * size), camDistance + 0.5f * size);
    } else {
        rt.setDepthRange(o.depthNear, o.depthFar);
    }

    // Same tile-by-tile workers as the GUI; on a terminal, show a percentage.
    RenderJob job(rt, scene, camera, o.width, o.height, 32, Vec3Df(0.f, 0.f, 0.f), o.threads);
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
        std::printf("model   %s: %zu object(s), %zu vertices, %zu triangles, size %.3f, centre (%.3f, %.3f, %.3f), up %s (%s)\n",
                    path.c_str(), scene.getObjects().size(), nv, nt, size, c[0], c[1], c[2], upAxisName(up),
                    upSource.c_str());
        std::printf("camera  pos (%.3f, %.3f, %.3f) dir (%.3f, %.3f, %.3f) fov %.1f yaw %.1f pitch %.1f\n",
                    camera.pos[0], camera.pos[1], camera.pos[2], camera.dir[0], camera.dir[1], camera.dir[2],
                    o.fovDeg, o.yaw, o.pitch);
        std::string effects = o.shadowSamples > 1 ? ", " + std::to_string(o.shadowSamples * o.shadowSamples) +
                                                     " shadow rays/light"
                                               : "";
        if (o.aoSamples > 0)
            effects += ", " + std::to_string(o.aoSamples * o.aoSamples) + " occlusion rays/hit";
        if ((o.reflectivity > 0.f || o.groundReflectivity > 0.f) && o.maxDepth > 0)
            effects += ", reflections up to depth " + std::to_string(o.maxDepth);
        std::printf("render  %ux%u %s, %u ray%s/px%s%s, %s, %u thread%s, in %.3fs: %lu/%lu rays hit (%.1f%%), hit distance [%.3f, %.3f]\n",
                    o.width, o.height, o.modeName.c_str(), o.aa * o.aa, o.aa > 1 ? "s" : "",
                    o.jitter ? " jittered" : "", effects.c_str(), o.bvh ? "bvh" : "brute force", job.threadCount(),
                    job.threadCount() > 1 ? "s" : "", st.seconds, st.hits, st.rays,
                    st.rays ? 100.0 * st.hits / st.rays : 0.0, st.minHitDist, st.maxHitDist);
        std::printf("wrote   %s\n", o.out.c_str());
    }
    return 0;
}
