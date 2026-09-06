// Golden-image regression: render the bundled models in every mode and
// compare against tests/golden/*.png. Regenerate with
//   build/raymini_tests --filter golden --update-golden
// after an intentional change, and look at the diff of the PNGs.
#include <cstdlib>
#include <filesystem>
#include <string>

#include "Camera.h"
#include "Fixtures.h"
#include "Image.h"
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

// `model` is a bare name (".off" assumed) or a file name with its extension.
void goldenModel(const char* model, float yawDeg, float pitchDeg) {
    Scene scene;
    scene.addObjectsFromFile(test::modelPath(model));
    scene.addDefaultLights();
    const BoundingBox& bbox = scene.getBoundingBox();
    const float size = bbox.getSize();
    const float distance = 2.f * size;
    const Camera camera = Camera::frame(bbox, kPi / 4.f, 1.f, 2.f, yawDeg, pitchDeg);

    RayTracer rt;
    rt.setDepthRange(distance - size / 2.f, distance + size / 2.f);
    for (const ModeSpec& m : kModes) {
        rt.setDebugMode(m.mode);
        const Image img = rt.render(scene, camera, kSize, kSize);
        const std::string name = std::filesystem::path(model).stem().string() + "_" + m.slug + ".png";
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
