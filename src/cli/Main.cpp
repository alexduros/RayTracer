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
#include <iostream>
#include <string>

#include "Camera.h"
#include "Image.h"
#include "RayTracer.h"
#include "Scene.h"

#ifndef RAYMINI_MODELS_DIR
#define RAYMINI_MODELS_DIR ""
#endif

namespace {

const char* kUsage = R"(Usage: raymini-cli <model.off> [options]

A bare name such as "teapot" resolves to the bundled models/ directory.

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
    const bool hasExt = given.size() > 4 && given.compare(given.size() - 4, 4, ".off") == 0;
    const std::string bundled = std::string(RAYMINI_MODELS_DIR) + "/" + given + (hasExt ? "" : ".off");
    if (fs::exists(bundled)) return bundled;
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
        scene.addObjectFromOFF(path);
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

    const Image image = rt.render(scene, camera, o.width, o.height);
    const RayTracer::Stats& st = rt.getLastStats();

    const auto parent = std::filesystem::path(o.out).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent);
    if (!image.save(o.out)) {
        std::cerr << "could not write " << o.out << "\n";
        return 1;
    }

    if (!o.quiet) {
        const Mesh& mesh = scene.getObjects()[0].getMesh();
        const Vec3Df c = bbox.getCenter();
        std::printf("model   %s: %zu vertices, %zu triangles, size %.3f, centre (%.3f, %.3f, %.3f)\n",
                    path.c_str(), mesh.getVertices().size(), mesh.getTriangles().size(), size, c[0], c[1], c[2]);
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
