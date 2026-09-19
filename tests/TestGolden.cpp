// Golden-image regression: render the bundled models in every mode and
// compare against tests/golden/*.png. Regenerate with
//   build/raymini_tests --filter golden --update-golden
// after an intentional change, and look at the diff of the PNGs.
#include <cstdlib>
#include <filesystem>
#include <string>

#include "Camera.h"
#include "Display.h"
#include "Fixtures.h"
#include "Image.h"
#include "Orientation.h"
#include "RayTracer.h"
#include "Scene.h"
#include "Test.h"

namespace {

const float kPi = 3.14159265358979f;
constexpr unsigned kSize = 96;
// Silhouette pixels can flip between compilers/CPUs (fused multiply-add), so
// allow a few pixels to differ; everything else must be within a small
// per-channel tolerance.
constexpr int kChannelTolerance = 4;
constexpr double kMaxDifferentFraction = 0.005;

struct ModeSpec {
    const char* slug;
    RayTracer::DebugMode mode;
};
const ModeSpec kModes[] = {
    {"lit", RayTracer::DebugMode::LIT},         {"ambient", RayTracer::DebugMode::AMBIENT},
    {"hitmask", RayTracer::DebugMode::HIT_MASK}, {"normals", RayTracer::DebugMode::NORMALS},
    {"depth", RayTracer::DebugMode::DEPTH},     {"objectid", RayTracer::DebugMode::OBJECT_ID},
    {"ao", RayTracer::DebugMode::AMBIENT_OCCLUSION},
};

bool matches(const Image& actual, const Image& expected, std::string& report) {
    if (actual.width() != expected.width() || actual.height() != expected.height() ||
        actual.channels() != expected.channels()) {
        report = "size mismatch";
        return false;
    }
    int maxDiff = 0;
    unsigned long different = 0;
    const unsigned long pixels = static_cast<unsigned long>(actual.width()) * actual.height();
    for (unsigned long i = 0; i < pixels * actual.channels(); i += actual.channels()) {
        int d = 0;
        for (int c = 0; c < actual.channels(); ++c)
            d = std::max(d, std::abs(int(actual.data()[i + c]) - int(expected.data()[i + c])));
        maxDiff = std::max(maxDiff, d);
        if (d > kChannelTolerance) ++different;
    }
    report = "max channel diff " + std::to_string(maxDiff) + ", " + std::to_string(different) + "/" +
             std::to_string(pixels) + " pixels differ by more than " + std::to_string(kChannelTolerance);
    return double(different) / double(pixels) <= kMaxDifferentFraction;
}

// What a golden turns on besides the defaults, encoded in its name: "_aa2" /
// "_aa2j", "_ground", "_soft4", "_mirror", "_ao4", "_glass", "_filmic".
struct Settings {
    unsigned int aaSamples = 1;
    bool jitter = false;
    bool ground = false;
    unsigned int shadowSamples = 1;
    float groundReflectivity = 0.f;
    unsigned int aoSamples = 0;  // radius 0.2 x the model size
    float transparency = 0.f;    // the model as glass of index 1.5
    bool filmic = false;         // Display::filmic () instead of the linear bytes of every other golden
};

Settings withGround() {
    Settings s;
    s.ground = true;
    return s;
}

// `model` is a bare name (".off" assumed) or a file name with its extension.
void goldenModel(const char* model, float yawDeg, float pitchDeg, const Settings& settings = Settings()) {
    const unsigned int aaSamples = settings.aaSamples, shadowSamples = settings.shadowSamples;
    const unsigned int aoSamples = settings.aoSamples;
    const bool jitter = settings.jitter, ground = settings.ground;
    const float groundReflectivity = settings.groundReflectivity, transparency = settings.transparency;
    Scene scene;
    scene.addObjectsFromFile(test::modelPath(model));
    scene.setUpAxis(resolveUpAxis(test::modelPath(model), scene));  // as the CLI and the viewer do
    scene.addDefaultLights();
    if (ground) scene.addGroundPlane();
    scene.setGroundReflectivity(groundReflectivity);
    if (transparency > 0.f) scene.setModelGlass(transparency, 1.5f);
    const BoundingBox& bbox = scene.getBoundingBox();
    const float size = bbox.getSize();
    const float distance = 2.f * size;
    const Camera camera = Camera::frame(bbox, kPi / 4.f, 1.f, 2.f, yawDeg, pitchDeg);

    RayTracer rt;
    rt.setDepthRange(distance - size / 2.f, distance + size / 2.f);
    rt.setAntiAliasing(aaSamples, jitter);
    rt.setShadowSamples(shadowSamples);
    rt.setAmbientOcclusion(aoSamples, 0.2f * size);
    if (settings.filmic) rt.setDisplay(Display::filmic());
    std::string stem = std::filesystem::path(model).stem().string();
    if (aaSamples > 1) stem += "_aa" + std::to_string(aaSamples) + (jitter ? "j" : "");
    if (ground) stem += "_ground";
    if (shadowSamples > 1) stem += "_soft" + std::to_string(shadowSamples);
    const bool litOnly =
        shadowSamples > 1 || groundReflectivity > 0.f || aoSamples > 0 || transparency > 0.f || settings.filmic;
    if (groundReflectivity > 0.f) stem += "_mirror";
    if (aoSamples > 0) stem += "_ao" + std::to_string(aoSamples);
    if (transparency > 0.f) stem += "_glass";
    if (settings.filmic) stem += "_filmic";
    for (const ModeSpec& m : kModes) {
        // The AO mode only when occlusion is on (it is all white otherwise).
        // With sampled or bouncing effects on, only the modes they change:
        // the others would repeat the goldens above.
        bool wanted = true;
        if (m.mode == RayTracer::DebugMode::AMBIENT_OCCLUSION)
            wanted = aoSamples > 0;
        else if (litOnly)
            wanted = m.mode == RayTracer::DebugMode::LIT;
        if (!wanted) continue;
        rt.setDebugMode(m.mode);
        const Image img = rt.render(scene, camera, kSize, kSize);
        const std::string name = stem + "_" + m.slug + ".png";
        const std::string goldenPath = test::goldenDir() + "/" + name;
        // Always keep what was rendered so a failure can be inspected.
        REQUIRE(img.save(test::outputDir() + "/golden_" + name));
        if (test::updateGolden()) {
            REQUIRE(img.save(goldenPath));
            continue;
        }
        Image expected;
        CHECK_MSG(expected.load(goldenPath), "missing " + goldenPath + " (run raymini_tests --update-golden)");
        if (!expected.isValid()) continue;
        std::string report;
        CHECK_MSG(matches(img, expected, report), name + ": " + report);
    }
}

}  // namespace

TEST_CASE("golden: teapot in every mode") { goldenModel("teapot", 25.f, 20.f); }
TEST_CASE("golden: ram in every mode") { goldenModel("ram", -35.f, 15.f); }
TEST_CASE("golden: cube.obj (six materials) in every mode") { goldenModel("cube.obj", 25.f, 20.f); }
TEST_CASE("golden: teapot with 2x2 supersampling") {
    Settings s;
    s.aaSamples = 2;
    goldenModel("teapot", 25.f, 20.f, s);
}
TEST_CASE("golden: teapot with 2x2 jittered supersampling") {
    Settings s;
    s.aaSamples = 2;
    s.jitter = true;
    goldenModel("teapot", 25.f, 20.f, s);
}
TEST_CASE("golden: teapot on its ground plane (shadows)") { goldenModel("teapot", 25.f, 20.f, withGround()); }
TEST_CASE("golden: teapot on its ground plane with 4x4 soft shadows") {
    Settings s = withGround();
    s.shadowSamples = 4;
    goldenModel("teapot", 25.f, 20.f, s);
}
TEST_CASE("golden: teapot on a mirror ground plane") {
    Settings s = withGround();
    s.groundReflectivity = 0.5f;
    goldenModel("teapot", 25.f, 20.f, s);
}
TEST_CASE("golden: teapot on its ground plane with 4x4 ambient occlusion") {
    Settings s = withGround();
    s.aoSamples = 4;
    goldenModel("teapot", 25.f, 20.f, s);
}
TEST_CASE("golden: a glass teapot on its ground plane") {
    Settings s = withGround();
    s.transparency = 1.f;
    goldenModel("teapot", 25.f, 20.f, s);
}
TEST_CASE("golden: teapot on its ground plane through the filmic display") {
    Settings s = withGround();
    s.filmic = true;
    goldenModel("teapot", 25.f, 20.f, s);
}
