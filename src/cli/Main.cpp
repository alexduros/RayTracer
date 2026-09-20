// ---------------------------------------------------------------------------
// raymini-cli: render an OFF model to a PNG without opening a window.
//
//   raymini-cli teapot --mode normals --size 512x512 --out renders/teapot.png
//
// Used by the golden-image tests, by CI and whenever a render is needed on a
// machine without a display.
// ---------------------------------------------------------------------------
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <chrono>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

#include "Camera.h"
#include "Display.h"
#include "HdrImage.h"
#include "Image.h"
#include "Orientation.h"
#include "Primitive.h"
#include "RayTracer.h"
#include "RenderJob.h"
#include "Scene.h"

#ifndef RAYMINI_MODELS_DIR
#define RAYMINI_MODELS_DIR ""
#endif
#ifndef RAYMINI_VERSION
#define RAYMINI_VERSION "0.0.0-dev"
#endif

namespace {

void printUsage(std::ostream& out) {
    out << R"(Usage: raymini-cli <model.off|model.obj> [options]

A bare name such as "teapot" or "cube" resolves to the bundled models/ directory.
OBJ files load their MTL materials (one object per material).

Options:
  --out <file>           output path, parent directories are created
                         (default renders/<model>_<mode>.png); a .hdr file keeps the linear
                         radiance (Radiance RGBE), before exposure, tone curve and encoding
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
  --transparency <g>     glass share of the model's materials, 0..1 (default: the file's, MTL d)
  --ior <n>              index of refraction of that glass, 1..4 (default: the file's Ni, else 1.5)
  --max-depth <n>        mirror and glass bounces followed per ray, 0..16 (default 8; 0 = none)
  --no-bvh               test every triangle (brute force) instead of the BVH, to compare
                         timings; the picture is identical
  --threads <n>          worker threads, 0 = one per core (default 0); the picture is identical
  --display <preset>     filmic (default): exposure measured on the image, ACES curve, sRGB;
                         linear: the historical conversion (no exposure, clipped at 1, linear
                         bytes), that of every render before experiment 9 and of the goldens.
                         The options below adjust the preset, in any order
  --exposure <ev|auto>   stops: a correction on top of the measured exposure when it is on,
                         the exposure itself when it is off; auto turns the measure on
  --auto-exposure <on|off>
                         measure the exposure that brings the log-average luminance to mid
                         grey, 0.18 (Reinhard et al.'s key)
  --tonemap <curve>      none (clip at 1), reinhard or aces
  --white <L>            Reinhard's white point: the radiance that maps to pure white
                         (default: none, highlights approach white without reaching it)
  --gamma <srgb|g>       output encoding: srgb or a power 1/g; 1 = linear bytes
  --sphere <x> <y> <z> <r> <kind>
                         add an analytic sphere: an equation, not triangles, so its
                         silhouette stays exact at any zoom. Position and radius in half
                         model sizes around the model's centre (like --light); kind is
                         matte, mirror or glass. Repeatable
  --ambient <a>          ambient intensity (default 0.15)
  --light <i> <x> <y> <z> <r> <g> <b> <intensity>
                         replace light i of the rig (0 key, 1 fill, 2 rim), or add one with
                         i = 3; position in half model sizes around the model's centre
  --color <r> <g> <b>    the model's colour (default: the file's, or orange)
  --specular <k>         the model's highlight strength
  --shininess <n>        the model's Blinn-Phong exponent
  --fov <deg>            vertical field of view (default 45)
  --yaw <deg>            orbit around the model about +Y (default 0: camera on +Z)
  --pitch <deg>          orbit elevation, -89..89 (default 0)
  --distance <factor>    camera distance = factor x model size (default 2)
  --depth <near> <far>   depth-mode range in world units (default: model extent)
  --quiet                only print errors
  --version              print the version and exit
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
    const RayTracer::ModeInfo& tone = RayTracer::toneMappingInfo();
    out << "\n--exposure / --tonemap / --white / --gamma  " << tone.name << "\n"
        << "      " << tone.principle << "\n"
        << "      Read: " << tone.reading << "\n"
        << "      Ref:  " << tone.reference << "\n";
    const RayTracer::ModeInfo& glass = RayTracer::refractionInfo();
    out << "\n--transparency / --ior / --max-depth  " << glass.name << "\n"
        << "      " << glass.principle << "\n"
        << "      Read: " << glass.reading << "\n"
        << "      Ref:  " << glass.reference << "\n";
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
    std::optional<float> transparency;  // model objects; empty: as the file says
    std::optional<float> ior;
    unsigned int maxDepth = 8;
    bool bvh = true;
    unsigned int threads = 0;  // 0 = one per core
    // The display: a preset, then whichever settings were given, in any order.
    std::string displayPreset = "filmic";
    std::optional<float> exposure;
    std::optional<bool> autoExposure;
    std::optional<Display::ToneMap> toneMap;
    std::optional<float> whitePoint;
    std::optional<Display::Encoding> encoding;
    float gamma = 2.2f;
    std::optional<float> ambient;
    struct LightOverride {
        int index;
        Vec3Df position, color;
        float intensity;
    };
    std::vector<LightOverride> lights;
    struct SphereSpec {
        Vec3Df centre;
        float radius;
        std::string kind;
    };
    std::vector<SphereSpec> spheres;
    std::optional<Vec3Df> color;
    std::optional<float> specularStrength, shininess;
    std::optional<UpAxis> up;  // empty = auto
    bool quiet = false;
};

Display displayFor(const Options& o) {
    Display d = o.displayPreset == "linear" ? Display::linear() : Display::filmic();
    if (o.autoExposure) d.autoExposure = *o.autoExposure;
    if (o.exposure) d.exposure = *o.exposure;
    if (o.toneMap) d.toneMap = *o.toneMap;
    if (o.whitePoint) d.whitePoint = *o.whitePoint;
    if (o.encoding) {
        d.encoding = *o.encoding;
        d.gamma = o.gamma;
    }
    return d;
}

std::string describe(const Display& d, bool autoExposure) {
    std::string s = d.encoding == Display::Encoding::SRGB     ? "sRGB"
                    : d.encoding == Display::Encoding::LINEAR ? "linear"
                                                              : "gamma " + std::to_string(d.gamma).substr(0, 4);
    s += d.toneMap == Display::ToneMap::ACES       ? ", aces"
         : d.toneMap == Display::ToneMap::REINHARD ? ", reinhard"
                                                   : ", clipped at 1";
    char ev[64];
    std::snprintf(ev, sizeof(ev), ", exposure %+.2f EV%s", d.exposure, autoExposure ? " (auto)" : "");
    return s + ev;
}

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
        } else if (a == "--version") {
            std::cout << "raymini " << RAYMINI_VERSION << "\n";
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
        } else if (a == "--display") {
            if (!(v = value(i, "--display"))) return false;
            o.displayPreset = v;
            if (o.displayPreset != "filmic" && o.displayPreset != "linear") {
                std::cerr << "--display must be filmic or linear\n";
                return false;
            }
        } else if (a == "--exposure") {
            if (!(v = value(i, "--exposure"))) return false;
            if (std::string(v) == "auto") {
                o.autoExposure = true;
            } else {
                o.exposure = static_cast<float>(std::atof(v));
            }
        } else if (a == "--auto-exposure") {
            if (!(v = value(i, "--auto-exposure"))) return false;
            if (std::string(v) != "on" && std::string(v) != "off") {
                std::cerr << "--auto-exposure must be on or off\n";
                return false;
            }
            o.autoExposure = std::string(v) == "on";
        } else if (a == "--tonemap") {
            if (!(v = value(i, "--tonemap"))) return false;
            const std::string t = v;
            if (t == "none") {
                o.toneMap = Display::ToneMap::NONE;
            } else if (t == "reinhard") {
                o.toneMap = Display::ToneMap::REINHARD;
            } else if (t == "aces") {
                o.toneMap = Display::ToneMap::ACES;
            } else {
                std::cerr << "--tonemap must be none, reinhard or aces\n";
                return false;
            }
        } else if (a == "--white") {
            if (!(v = value(i, "--white"))) return false;
            o.whitePoint = static_cast<float>(std::atof(v));
            if (*o.whitePoint <= 0.f) {
                std::cerr << "--white must be more than 0\n";
                return false;
            }
        } else if (a == "--gamma") {
            if (!(v = value(i, "--gamma"))) return false;
            if (std::string(v) == "srgb") {
                o.encoding = Display::Encoding::SRGB;
            } else {
                const float g = static_cast<float>(std::atof(v));
                if (g < 0.1f || g > 10.f) {
                    std::cerr << "--gamma must be srgb or a number between 0.1 and 10\n";
                    return false;
                }
                o.encoding = g == 1.f ? Display::Encoding::LINEAR : Display::Encoding::GAMMA;
                o.gamma = g;
            }
        } else if (a == "--sphere") {
            if (i + 5 >= argc) {
                std::cerr << "--sphere needs <x> <y> <z> <r> <matte|mirror|glass>\n";
                return false;
            }
            Options::SphereSpec sphere;
            for (int k = 0; k < 3; ++k) sphere.centre[k] = static_cast<float>(std::atof(argv[++i]));
            sphere.radius = static_cast<float>(std::atof(argv[++i]));
            sphere.kind = argv[++i];
            if (sphere.radius <= 0.f) {
                std::cerr << "--sphere: the radius must be more than 0\n";
                return false;
            }
            if (sphere.kind != "matte" && sphere.kind != "mirror" && sphere.kind != "glass") {
                std::cerr << "--sphere: the kind must be matte, mirror or glass\n";
                return false;
            }
            o.spheres.push_back(sphere);
        } else if (a == "--ambient") {
            if (!(v = value(i, "--ambient"))) return false;
            o.ambient = static_cast<float>(std::atof(v));
            if (*o.ambient < 0.f) {
                std::cerr << "--ambient must be 0 or more\n";
                return false;
            }
        } else if (a == "--light") {
            if (i + 8 >= argc) {
                std::cerr << "--light needs <i> <x> <y> <z> <r> <g> <b> <intensity>\n";
                return false;
            }
            Options::LightOverride l;
            l.index = std::atoi(argv[++i]);
            for (int k = 0; k < 3; ++k) l.position[k] = static_cast<float>(std::atof(argv[++i]));
            for (int k = 0; k < 3; ++k) l.color[k] = static_cast<float>(std::atof(argv[++i]));
            l.intensity = static_cast<float>(std::atof(argv[++i]));
            if (l.index < 0 || l.intensity < 0.f) {
                std::cerr << "--light: the index and the intensity must be 0 or more\n";
                return false;
            }
            o.lights.push_back(l);
        } else if (a == "--color") {
            if (i + 3 >= argc) {
                std::cerr << "--color needs <r> <g> <b>\n";
                return false;
            }
            Vec3Df c;
            for (int k = 0; k < 3; ++k) c[k] = static_cast<float>(std::atof(argv[++i]));
            o.color = c;
        } else if (a == "--specular") {
            if (!(v = value(i, "--specular"))) return false;
            o.specularStrength = static_cast<float>(std::atof(v));
        } else if (a == "--shininess") {
            if (!(v = value(i, "--shininess"))) return false;
            o.shininess = std::max(1.f, static_cast<float>(std::atof(v)));
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
        } else if (a == "--transparency") {
            if (!(v = value(i, "--transparency"))) return false;
            o.transparency = static_cast<float>(std::atof(v));
            if (*o.transparency < 0.f || *o.transparency > 1.f) {
                std::cerr << "--transparency must be between 0 and 1\n";
                return false;
            }
        } else if (a == "--ior") {
            if (!(v = value(i, "--ior"))) return false;
            o.ior = static_cast<float>(std::atof(v));
            if (*o.ior < 1.f || *o.ior > 4.f) {
                std::cerr << "--ior must be between 1 and 4\n";
                return false;
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

// "teapot" or "teapot.off" -> a models directory, unless the path exists as
// given. Looked for beside the current directory and beside the executable
// (where the release archive keeps them), then in the directory this binary
// was built against, which only exists on the machine that built it.
std::string resolveModel(const std::string& given, const char* argv0) {
    namespace fs = std::filesystem;
    if (fs::exists(given)) return given;

    std::vector<fs::path> dirs = {fs::path("models")};
    if (argv0 && *argv0) {
        std::error_code ec;
        const fs::path exe = fs::weakly_canonical(fs::path(argv0), ec);
        if (!ec && exe.has_parent_path()) {
            dirs.push_back(exe.parent_path() / "models");         // next to the binary
            dirs.push_back(exe.parent_path() / ".." / "models");  // bin/ inside the archive
        }
    }
    dirs.push_back(fs::path(RAYMINI_MODELS_DIR));

    for (const fs::path& dir : dirs) {
        if (dir.empty()) continue;
        for (const char* ext : {"", ".off", ".obj"}) {
            const fs::path candidate = dir / (given + ext);
            if (fs::exists(candidate)) return candidate.string();
        }
    }
    return given;  // let the loader report the error with the name the user typed
}

}  // namespace

int main(int argc, char** argv) {
    Options o;
    if (!parseArgs(argc, argv, o)) return 2;

    const std::string path = resolveModel(o.model, argc > 0 ? argv[0] : nullptr);
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
    // Analytic primitives are placed in scene coordinates, after the model is
    // oriented and before the lights, so the rig and the framing see them.
    auto addSpheres = [&scene](const std::vector<Options::SphereSpec>& specs) {
        if (specs.empty()) return;
        const BoundingBox& box = scene.getBoundingBox();
        const float half = std::max(box.getSize(), 1e-3f) / 2.f;
        const Vec3Df centre = box.getCenter();
        for (const Options::SphereSpec& spec : specs) {
            Material material = Scene::defaultMaterial();
            if (spec.kind == "mirror") {
                // Dark under the mirror, or its own colour washes the reflection out.
                material = Material(1.f, 0.3f, Vec3Df(0.06f, 0.06f, 0.07f), 200.f, 0.95f);
            } else if (spec.kind == "glass") {
                material = Material(1.f, 0.2f, Vec3Df(1.f, 1.f, 1.f), 128.f);
                material.setTransparency(1.f);
            }
            scene.addObject(Object(std::make_shared<Sphere>(centre + half * spec.centre, half * spec.radius),
                                   material));
        }
    };

    // Orient before placing lights: the scene is Y-up, the file may not be.
    std::string upSource = "option";
    const UpAxis up = o.up ? *o.up : resolveUpAxis(path, scene, &upSource);
    scene.setUpAxis(up);
    addSpheres(o.spheres);
    scene.addDefaultLights();
    if (o.lightRadius >= 0.f) scene.setLightRadius(o.lightRadius);
    // Light overrides, in the rig's own units: half the model's size around its centre.
    for (const Options::LightOverride& l : o.lights) {
        std::vector<Light>& lights = scene.getLights();
        const BoundingBox& box = scene.getBoundingBox();
        const float half = std::max(box.getSize(), 1e-3f) / 2.f;
        const float radius = lights.empty() ? Scene::kDefaultLightRadius * 2.f * half : lights[0].getRadius();
        const Light light(box.getCenter() + half * l.position, l.color, l.intensity, radius);
        if (l.index < static_cast<int>(lights.size())) {
            lights[static_cast<size_t>(l.index)] = light;
        } else if (l.index == static_cast<int>(lights.size())) {
            lights.push_back(light);
        } else {
            std::cerr << "--light " << l.index << ": the rig has " << lights.size()
                      << " lights, so the index must be at most " << lights.size() << "\n";
            return 2;
        }
    }
    if (o.ground) scene.addGroundPlane();  // a backdrop: framing below still follows the model
    scene.setModelReflectivity(o.reflectivity);
    scene.setGroundReflectivity(o.groundReflectivity);
    // Glass only when asked: an OBJ's MTL may already make some of it glass.
    for (Object& object : scene.getObjects()) {
        if (object.isBackdrop()) continue;
        Material& m = object.getMaterial();
        if (o.transparency) m.setTransparency(*o.transparency);
        if (o.ior) m.setIor(*o.ior);
        if (o.color) m.setColor(*o.color);
        if (o.specularStrength) m.setSpecular(*o.specularStrength);
        if (o.shininess) m.setShininess(*o.shininess);
    }

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
    const Display display = displayFor(o);
    rt.setDisplay(display);
    if (o.ambient) rt.setAmbientIntensity(*o.ambient);
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
    const HdrImage hdr = job.hdrSnapshot();
    const RayTracer::Stats st = job.stats();
    // The display comes last: an automatic exposure needs the whole image.
    const Display shown = display.resolved(hdr);

    const auto parent = std::filesystem::path(o.out).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent);
    std::string ext = std::filesystem::path(o.out).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
    const bool saved = ext == ".hdr" ? hdr.save(o.out) : shown.apply(hdr).save(o.out);
    if (!saved) {
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
        bool glassy = false;
        for (const Object& object : scene.getObjects()) glassy = glassy || object.getMaterial().getTransparency() > 0.f;
        if ((o.reflectivity > 0.f || o.groundReflectivity > 0.f || glassy) && o.maxDepth > 0)
            effects += std::string(", ") + (glassy ? "glass and reflections" : "reflections") + " up to depth " +
                       std::to_string(o.maxDepth);
        std::printf("render  %ux%u %s, %u ray%s/px%s%s, %s, %u thread%s, in %.3fs: %lu/%lu rays hit (%.1f%%), hit distance [%.3f, %.3f]\n",
                    o.width, o.height, o.modeName.c_str(), o.aa * o.aa, o.aa > 1 ? "s" : "",
                    o.jitter ? " jittered" : "", effects.c_str(), o.bvh ? "bvh" : "brute force", job.threadCount(),
                    job.threadCount() > 1 ? "s" : "", st.seconds, st.hits, st.rays,
                    st.rays ? 100.0 * st.hits / st.rays : 0.0, st.minHitDist, st.maxHitDist);
        std::printf("display %s\n", ext == ".hdr" ? "none: linear radiance in Radiance RGBE"
                                                   : describe(shown, display.autoExposure).c_str());
        std::printf("wrote   %s\n", o.out.c_str());
    }
    return 0;
}
