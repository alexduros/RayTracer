#include <cmath>
#include <cstring>
#include <string>

#include "Camera.h"
#include "Display.h"
#include "Fixtures.h"
#include "HdrImage.h"
#include "Image.h"
#include "Light.h"
#include "RayTracer.h"
#include "RenderJob.h"
#include "Scene.h"
#include "Test.h"

namespace {

const float kPi = 3.14159265358979f;

unsigned char byteOf(const Display& d, float linear) {
    unsigned char rgb[3];
    d.toBytes(Vec3Df(linear, linear, linear), rgb);
    return rgb[0];
}

Display srgb() {
    Display d;
    d.encoding = Display::Encoding::SRGB;
    return d;
}

float luminance(const Vec3Df& c) { return 0.2126f * c[0] + 0.7152f * c[1] + 0.0722f * c[2]; }

}  // namespace

TEST_CASE("display: the default is the old linear conversion, byte for byte") {
    const Display d;
    for (int k = -10; k <= 300; ++k) {
        const float c = static_cast<float>(k) / 255.f;
        const int expected = std::max(0, std::min(255, static_cast<int>(c * 255.f + 0.5f)));
        CHECK_MSG(byteOf(d, c) == expected, "value " + std::to_string(c));
    }
    CHECK_EQ(byteOf(d, 0.5f), 128);
}

TEST_CASE("display: sRGB encodes linear 0.5 as 188, and exposure scales before encoding") {
    const Display s = srgb();
    CHECK_EQ(byteOf(s, 0.f), 0);
    CHECK_EQ(byteOf(s, 0.5f), 188);  // 1.055 x 0.5^(1/2.4) - 0.055 = 0.7354
    CHECK_EQ(byteOf(s, 1.f), 255);
    CHECK_EQ(byteOf(s, 0.002f), static_cast<int>(12.92f * 0.002f * 255.f + 0.5f));  // the linear toe
    // The two pieces of the curve meet at 0.0031308.
    CHECK_CLOSE(s.map(Vec3Df(0.0031308f, 0.f, 0.f))[0], 0.04045f, 1e-4);
    unsigned char previous = 0;
    for (int k = 0; k <= 1000; ++k) {
        const unsigned char b = byteOf(s, static_cast<float>(k) / 1000.f);
        CHECK(b >= previous);
        previous = b;
    }

    // Exposure is in stops: +1 doubles the linear value, -2 quarters it.
    Display brighter = s, darker = s;
    brighter.exposure = 1.f;
    darker.exposure = -2.f;
    for (float c : {0.01f, 0.1f, 0.3f, 0.45f}) {
        CHECK_CLOSE(brighter.map(Vec3Df(c, c, c))[0], s.map(Vec3Df(2.f * c, 0.f, 0.f))[0], 1e-6);
        CHECK_CLOSE(darker.map(Vec3Df(c, c, c))[0], s.map(Vec3Df(0.25f * c, 0.f, 0.f))[0], 1e-6);
    }

    // A plain gamma for completeness: 0.5 ^ (1 / 2.2).
    Display g;
    g.encoding = Display::Encoding::GAMMA;
    g.gamma = 2.2f;
    CHECK_CLOSE(g.map(Vec3Df(0.5f, 0.5f, 0.5f))[0], std::pow(0.5f, 1.f / 2.2f), 1e-6);
}

TEST_CASE("display: with a tone curve, light above 1 keeps its gradations instead of clipping to 255") {
    for (Display::ToneMap curve : {Display::ToneMap::REINHARD, Display::ToneMap::ACES}) {
        Display d = srgb();
        d.toneMap = curve;
        const std::string name = curve == Display::ToneMap::REINHARD ? "reinhard" : "aces";
        unsigned char previous = 0;
        for (float c : {0.5f, 1.f, 1.5f, 3.f, 6.f}) {
            const unsigned char b = byteOf(d, c);
            CHECK_MSG(b > previous && b < 255, name + ": " + std::to_string(c) + " -> " + std::to_string(b));
            previous = b;
        }
        CHECK_EQ(byteOf(d, 0.f), 0);
    }
    // Without one, everything above 1 is the same flat 255.
    const Display none = srgb();
    CHECK_EQ(byteOf(none, 1.5f), 255);
    CHECK_EQ(byteOf(none, 6.f), 255);
}

TEST_CASE("display: Reinhard maps luminance L to L / (1 + L), keeping the hue, burning out at the white point") {
    Display d;
    d.toneMap = Display::ToneMap::REINHARD;
    CHECK_CLOSE(d.map(Vec3Df(1.f, 1.f, 1.f))[0], 0.5f, 1e-6);
    CHECK_CLOSE(d.map(Vec3Df(3.f, 3.f, 3.f))[0], 0.75f, 1e-6);
    // Colour ratios survive: only the luminance is compressed.
    const Vec3Df c(0.8f, 0.4f, 0.2f), mapped = d.map(c);
    CHECK_CLOSE(mapped[0] / mapped[1], 2.f, 1e-5);
    CHECK_CLOSE(mapped[1] / mapped[2], 2.f, 1e-5);
    const float l = luminance(c);
    CHECK_CLOSE(luminance(mapped), l / (1.f + l), 1e-5);
    // With a white point, L_white maps to exactly 1 (Reinhard et al.'s equation 4).
    d.whitePoint = 4.f;
    CHECK_CLOSE(d.map(Vec3Df(4.f, 4.f, 4.f))[0], 1.f, 1e-6);
    CHECK_CLOSE(d.map(Vec3Df(1.f, 1.f, 1.f))[0], (1.f + 1.f / 16.f) / 2.f, 1e-6);
}

TEST_CASE("display: the ACES curve starts at 0, rises steadily and settles at 1") {
    Display d;
    d.toneMap = Display::ToneMap::ACES;
    CHECK_CLOSE(d.map(Vec3Df(0.f, 0.f, 0.f))[0], 0.f, 1e-6);
    float previous = -1.f;
    for (int k = 0; k <= 100; ++k) {
        const float v = d.map(Vec3Df(0.2f * static_cast<float>(k), 0.f, 0.f))[0];
        CHECK(v >= previous && v <= 1.f);
        previous = v;
    }
    CHECK(d.map(Vec3Df(20.f, 0.f, 0.f))[0] > 0.99f);
    // Narkowicz's fit: 0.18 (mid grey) lands near 0.27.
    CHECK_CLOSE(d.map(Vec3Df(0.18f, 0.f, 0.f))[0], 0.18f * (2.51f * 0.18f + 0.03f) / (0.18f * (2.43f * 0.18f + 0.59f) + 0.14f),
                1e-6);
}

TEST_CASE("display: automatic exposure brings the log-average luminance to the key, 0.18") {
    HdrImage flat(8, 4);
    flat.fill(Vec3Df(0.72f, 0.72f, 0.72f));
    CHECK_CLOSE(Display::meteredExposure(flat), -2.f, 1e-5);  // 0.72 x 2^-2 = 0.18
    HdrImage mixed(2, 1);
    mixed.set(0, 0, Vec3Df(0.02f, 0.02f, 0.02f));
    mixed.set(1, 0, Vec3Df(2.f, 2.f, 2.f));  // log-average 0.2, not the arithmetic 1.01
    CHECK_CLOSE(Display::meteredExposure(mixed), std::log2(0.18f / 0.2f), 1e-4);
    HdrImage black(4, 4);
    CHECK_CLOSE(Display::meteredExposure(black), 0.f, 0.f);  // nothing to measure: leave it
}

TEST_CASE("display: the filmic preset measures its exposure, and a set exposure corrects on top of it") {
    const Display f = Display::filmic();
    CHECK(f.autoExposure && f.toneMap == Display::ToneMap::ACES && f.encoding == Display::Encoding::SRGB);
    HdrImage flat(4, 4);
    flat.fill(Vec3Df(0.72f, 0.72f, 0.72f));
    CHECK_CLOSE(f.resolved(flat).exposure, -2.f, 1e-5);
    CHECK(!f.resolved(flat).autoExposure);
    Display brighter = f;
    brighter.exposure = 1.f;  // one stop over the metered exposure
    CHECK_CLOSE(brighter.resolved(flat).exposure, -1.f, 1e-5);
    // apply () is map () with the resolved exposure.
    unsigned char rgb[3];
    f.resolved(flat).toBytes(Vec3Df(0.72f, 0.72f, 0.72f), rgb);
    const Image image = f.apply(flat);
    unsigned char r, g, b;
    image.getPixel(2, 2, r, g, b);
    CHECK(r == rgb[0] && g == rgb[1] && b == rgb[2]);
    // The linear preset is the default: no measure, nothing changed.
    const Display l = Display::linear();
    CHECK(!l.autoExposure && l.toneMap == Display::ToneMap::NONE && l.encoding == Display::Encoding::LINEAR);
    CHECK_CLOSE(l.resolved(flat).exposure, 0.f, 0.f);
}

TEST_CASE("display: HDR images keep radiance above 1 and survive a Radiance .hdr round trip") {
    // Three lights on a white plane add up above 1: the float buffer keeps it.
    Scene s = fixtures::sceneOf(fixtures::quad(0.f, 10.f));
    for (int i = 0; i < 3; ++i) s.addLight(Light(Vec3Df(0.f, 0.f, 5.f), Vec3Df(1.f, 1.f, 1.f), 1.f, 0.f));
    const Camera cam = Camera::lookAt(Vec3Df(0.f, 0.f, 5.f), Vec3Df(0.f, 0.f, 0.f), Vec3Df(0.f, 1.f, 0.f), kPi / 4.f, 1.f);
    RayTracer rt;
    rt.setSpecularEnabled(false);
    const HdrImage hdr = rt.renderHdr(s, cam, 16, 16);
    CHECK_CLOSE(hdr.get(8, 8)[0], 0.15f + 3.f * 1.f, 0.02f);  // ambient + three lights head-on
    // The 8-bit render is that buffer through the display, pixel for pixel.
    const Image bytes = rt.render(s, cam, 16, 16);
    const Image mapped = rt.getDisplay().apply(hdr);
    REQUIRE(bytes.sizeInBytes() == mapped.sizeInBytes());
    CHECK(std::memcmp(bytes.data(), mapped.data(), bytes.sizeInBytes()) == 0);

    // RGBE (Ward's "Real Pixels"): a shared exponent, 8-bit mantissas.
    HdrImage ramp(16, 1);
    for (int x = 0; x < 16; ++x) {
        const float v = std::pow(2.f, static_cast<float>(x) - 8.f);  // 1/256 .. 128
        ramp.set(x, 0, Vec3Df(v, 0.5f * v, 0.25f * v));
    }
    const std::string path = test::outputDir() + "/ramp.hdr";
    REQUIRE(ramp.save(path));
    HdrImage back;
    REQUIRE(back.load(path));
    REQUIRE(back.width() == 16 && back.height() == 1);
    for (int x = 0; x < 16; ++x)
        for (int c = 0; c < 3; ++c)
            CHECK_MSG(std::fabs(back.get(x, 0)[c] - ramp.get(x, 0)[c]) <= ramp.get(x, 0)[0] / 128.f,
                      "texel " + std::to_string(x) + " channel " + std::to_string(c));
}

TEST_CASE("display: tiles and threads give the same buffer and the same bytes, whatever the display") {
    Scene s;
    s.addObjectsFromFile(test::modelPath("teapot"));
    s.addDefaultLights();
    s.addGroundPlane();
    const Camera cam = Camera::frame(s.getBoundingBox(), kPi / 4.f, 1.f, 2.f, 25.f, 20.f);
    RayTracer rt;
    Display d = srgb();
    d.toneMap = Display::ToneMap::ACES;
    d.exposure = 0.5f;
    rt.setDisplay(d);
    const HdrImage reference = rt.renderHdr(s, cam, 48, 40);
    RenderJob job(rt, s, cam, 48, 40, 16, Vec3Df(0.f, 0.f, 0.f), 3);
    job.start();
    job.wait();
    const HdrImage tiled = job.hdrSnapshot();
    REQUIRE(tiled.width() == 48 && tiled.height() == 40);
    bool same = true;
    for (int y = 0; y < 40; ++y)
        for (int x = 0; x < 48; ++x) same = same && tiled.get(x, y) == reference.get(x, y);
    CHECK(same);
    const Image bytes = job.snapshot(), expected = d.apply(reference);
    CHECK(std::memcmp(bytes.data(), expected.data(), bytes.sizeInBytes()) == 0);
}
