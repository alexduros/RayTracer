#include <array>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "Camera.h"
#include "Environment.h"
#include "Fixtures.h"
#include "HdrImage.h"
#include "Image.h"
#include "Material.h"
#include "Primitive.h"
#include "RayTracer.h"
#include "RenderJob.h"
#include "Scene.h"
#include "Test.h"

namespace {

const float kPi = 3.14159265358979f;

using Texel = std::array<unsigned char, 3>;
const Texel kBlack{{0, 0, 0}}, kRed{{255, 0, 0}}, kGreen{{0, 255, 0}}, kBlue{{0, 0, 255}};
const Texel kYellow{{255, 255, 0}}, kMagenta{{255, 0, 255}}, kCyan{{0, 255, 255}}, kWhite{{255, 255, 255}};

/// Write a panorama and read it back as the world, the way the CLI does.
/// Channels at 0 or 255 decode to exactly 0 and 1.
std::shared_ptr<const Environment> worldOf(const std::string& name, int w, int h, const std::vector<Texel>& texels) {
    Image image(w, h, Image::RGB888);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const Texel& t = texels[static_cast<size_t>(y) * w + x];
            image.setPixel(x, y, t[0], t[1], t[2]);
        }
    const std::string path = test::outputDir() + "/" + name;
    REQUIRE(image.save(path));
    return Environment::load(path);
}

/// A world with no two directions alike: a smooth picture, 32 x 16 texels.
std::shared_ptr<const Environment> paintedWorld() {
    const int w = 32, h = 16;
    std::vector<Texel> texels;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            texels.push_back({{static_cast<unsigned char>(255 * x / (w - 1)),
                               static_cast<unsigned char>(255 * y / (h - 1)),
                               static_cast<unsigned char>(128 + 127 * std::sin(0.7f * static_cast<float>(x + 2 * y)))}});
    return worldOf("painted_world.png", w, h, texels);
}

void checkColor(const Vec3Df& actual, const Vec3Df& expected, float tolerance, const std::string& what) {
    for (int c = 0; c < 3; ++c)
        CHECK_MSG(std::fabs(actual[c] - expected[c]) <= tolerance,
                  what + ", channel " + std::to_string(c) + ": " + std::to_string(actual[c]) + " instead of " +
                      std::to_string(expected[c]));
}

Vec3Df traced(const RayTracer& rt, const Scene& s, const Vec3Df& from, const Vec3Df& direction) {
    RayTracer::Stats st;
    return rt.trace(s, Ray(from, direction), st);
}

/// A unit sphere at the origin, and nothing else: no light, so whatever
/// colour comes back was brought by a ray that left the scene.
Scene sphereIn(const std::shared_ptr<const Environment>& world, const Material& material) {
    Scene s;
    s.addObject(Object(std::make_shared<Sphere>(Vec3Df(0.f, 0.f, 0.f), 1.f), material));
    s.setEnvironment(world);
    return s;
}

Material mirror() { return Material(1.f, 0.f, Vec3Df(1.f, 1.f, 1.f), 32.f, 1.f); }

Material glass(float ior) {
    Material m(1.f, 0.f, Vec3Df(1.f, 1.f, 1.f));
    m.setTransparency(1.f);
    m.setIor(ior);
    return m;
}

bool samePixels(const Image& a, const Image& b) {
    return a.sizeInBytes() == b.sizeInBytes() && std::memcmp(a.data(), b.data(), a.sizeInBytes()) == 0;
}

}  // namespace

TEST_CASE("environment: a direction falls at its azimuth across and its polar angle down") {
    // Blinn & Newell plot "azimuthal angle as abscissa and polar angle as
    // ordinate". Ours: the middle of the picture is -Z, where the default
    // camera looks, and +X is a quarter of a turn to its right.
    struct Known {
        Vec3Df direction;
        float u, v;
        const char* what;
    };
    const float s = std::sqrt(0.5f);
    for (const Known& k : {Known{Vec3Df(0.f, 0.f, -1.f), 0.5f, 0.5f, "-Z is the centre"},
                           Known{Vec3Df(1.f, 0.f, 0.f), 0.75f, 0.5f, "+X is a quarter turn to the right"},
                           Known{Vec3Df(-1.f, 0.f, 0.f), 0.25f, 0.5f, "-X a quarter turn to the left"},
                           Known{Vec3Df(0.f, s, -s), 0.5f, 0.25f, "45 degrees up is halfway to the top"},
                           Known{Vec3Df(0.f, -s, -s), 0.5f, 0.75f, "45 degrees down halfway to the bottom"},
                           Known{Vec3Df(s, 0.f, -s), 0.625f, 0.5f, "45 degrees right an eighth of the width"}}) {
        float u = -1.f, v = -1.f;
        Environment::toMap(k.direction, u, v);
        CHECK_MSG(std::fabs(u - k.u) < 1e-6f && std::fabs(v - k.v) < 1e-6f,
                  std::string(k.what) + ": (" + std::to_string(u) + ", " + std::to_string(v) + ")");
    }
    float u = -1.f, v = -1.f;
    Environment::toMap(Vec3Df(0.f, 1.f, 0.f), u, v);
    CHECK_CLOSE(v, 0.f, 1e-6);  // straight up: the top row, whatever the column
    Environment::toMap(Vec3Df(0.f, -1.f, 0.f), u, v);
    CHECK_CLOSE(v, 1.f, 1e-6);
    // Behind the camera is where the picture's two edges meet.
    Environment::toMap(Vec3Df(0.f, 0.f, 1.f), u, v);
    CHECK_CLOSE(std::fabs(u - 0.5f), 0.5f, 1e-6);
    CHECK_CLOSE(v, 0.5f, 1e-6);

    // Only the direction counts, not the length, and the way back is exact.
    float u2 = -1.f, v2 = -1.f;
    Environment::toMap(Vec3Df(0.3f, 0.5f, -0.2f), u, v);
    Environment::toMap(7.f * Vec3Df(0.3f, 0.5f, -0.2f), u2, v2);
    CHECK_CLOSE(u, u2, 1e-6);
    CHECK_CLOSE(v, v2, 1e-6);
    for (const Vec3Df& given : {Vec3Df(0.3f, 0.5f, -0.2f), Vec3Df(-0.9f, -0.1f, 0.4f), Vec3Df(0.1f, -0.8f, 0.6f)}) {
        Vec3Df d = given;
        d.normalize();
        Environment::toMap(d, u, v);
        const Vec3Df back = Environment::toDirection(u, v);
        CHECK_MSG(Vec3Df::distance(back, d) < 1e-5f, "there and back");
    }
}

TEST_CASE("environment: a ray leaving along d reads the texel at (atan2, acos) of d") {
    // Eight texels, eight colours: each direction through a texel's centre
    // must bring that texel back and no other.
    const std::vector<Texel> texels = {kRed, kGreen, kBlue, kWhite, kYellow, kMagenta, kCyan, kBlack};
    const auto world = worldOf("eight_texels.png", 4, 2, texels);
    REQUIRE(world != nullptr);
    CHECK_EQ(world->width(), 4);
    CHECK_EQ(world->height(), 2);
    for (int row = 0; row < 2; ++row)
        for (int column = 0; column < 4; ++column) {
            // The centre of the texel, as angles: no map is consulted here.
            const float azimuth = ((static_cast<float>(column) + 0.5f) / 4.f - 0.5f) * 2.f * kPi;
            const float polar = (static_cast<float>(row) + 0.5f) / 2.f * kPi;
            const Vec3Df d(std::sin(polar) * std::sin(azimuth), std::cos(polar), -std::sin(polar) * std::cos(azimuth));
            const Texel& t = texels[static_cast<size_t>(row) * 4 + column];
            checkColor(world->sample(d), Vec3Df(t[0] / 255.f, t[1] / 255.f, t[2] / 255.f), 1e-4f,
                       "texel " + std::to_string(column) + ", " + std::to_string(row));
        }
    // In words: up and to the left of where the camera looks is the second
    // texel of the top row.
    checkColor(world->sample(Vec3Df(-1.f, 1.f, -1.f)), Vec3Df(0.f, 1.f, 0.f), 0.2f, "up and left leans green");
}

TEST_CASE("environment: the picture closes on itself in longitude, and its poles stay apart") {
    // Four columns, red to white. Straight behind the camera the two edges of
    // the picture meet: the read is half the last column, half the first.
    const auto ring = worldOf("ring.png", 4, 1, {kRed, kGreen, kBlue, kWhite});
    REQUIRE(ring != nullptr);
    checkColor(ring->sample(Vec3Df(0.f, 0.f, 1.f)), Vec3Df(1.f, 0.5f, 0.5f), 1e-4f, "the seam");
    // ... and nothing jumps there: a hair to either side reads the same.
    checkColor(ring->sample(Vec3Df(1e-3f, 0.f, 1.f)), ring->sample(Vec3Df(-1e-3f, 0.f, 1.f)), 2e-3f,
               "across the seam");

    // A red sky over a blue floor. Straight up is red and only red: a
    // texture wraps top into bottom, the world must not.
    const auto poles = worldOf("poles.png", 2, 2, {kRed, kRed, kBlue, kBlue});
    REQUIRE(poles != nullptr);
    checkColor(poles->sample(Vec3Df(0.f, 1.f, 0.f)), Vec3Df(1.f, 0.f, 0.f), 1e-4f, "the zenith");
    checkColor(poles->sample(Vec3Df(0.f, -1.f, 0.f)), Vec3Df(0.f, 0.f, 1.f), 1e-4f, "the nadir");
    checkColor(poles->sample(Vec3Df(0.2f, 0.98f, -0.1f)), Vec3Df(1.f, 0.f, 0.f), 1e-4f, "near the zenith");
    // The horizon is where they blend.
    checkColor(poles->sample(Vec3Df(0.f, 0.f, -1.f)), Vec3Df(0.5f, 0.f, 0.5f), 1e-4f, "the horizon");
}

TEST_CASE("environment: a ray that escapes brings the world back, in Lit mode only") {
    const auto world = paintedWorld();
    REQUIRE(world != nullptr);
    // A wall in front of the camera, with the world around it.
    Scene s = fixtures::sceneOf(fixtures::quad(0.f, 1.f));
    s.setEnvironment(world);
    RayTracer rt;
    rt.setBackgroundColor(Vec3Df(0.1f, 0.2f, 0.3f));
    const Vec3Df eye(0.f, 0.f, 5.f);

    // Past the wall, in any direction: the map along that direction, whatever
    // the origin (the world is infinitely far).
    for (const Vec3Df& given : {Vec3Df(0.9f, 0.1f, -1.f), Vec3Df(-0.5f, 0.8f, -1.f), Vec3Df(0.f, -1.f, 0.f),
                                Vec3Df(0.3f, 0.2f, 1.f)}) {
        Vec3Df d = given;
        d.normalize();
        const Vec3Df seen = traced(rt, s, eye, d);
        checkColor(seen, world->sample(d), 1e-5f, "an escaping ray");
        checkColor(traced(rt, s, eye + Vec3Df(40.f, -7.f, 3.f), d), seen, 1e-5f, "from somewhere else");
    }
    // The wall hides the world behind it.
    RayTracer::Stats stats;
    rt.trace(s, Ray(eye, Vec3Df(0.f, 0.f, -1.f)), stats);
    CHECK_EQ(stats.hits, 1ul);
    // An escaping ray is a ray, not a hit.
    rt.trace(s, Ray(eye, Vec3Df(0.f, 1.f, 0.f)), stats);
    CHECK_EQ(stats.rays, 2ul);
    CHECK_EQ(stats.hits, 1ul);

    // The analysis modes show values, not light: their background stays flat.
    const Vec3Df up(0.f, 1.f, 0.f);
    for (RayTracer::DebugMode mode :
         {RayTracer::DebugMode::AMBIENT, RayTracer::DebugMode::HIT_MASK, RayTracer::DebugMode::NORMALS,
          RayTracer::DebugMode::DEPTH, RayTracer::DebugMode::OBJECT_ID, RayTracer::DebugMode::AMBIENT_OCCLUSION,
          RayTracer::DebugMode::UV}) {
        rt.setDebugMode(mode);
        CHECK_MSG(traced(rt, s, eye, up) == Vec3Df(0.1f, 0.2f, 0.3f), RayTracer::info(mode).slug);
    }
    // And with no world, Lit falls back on it too: nothing changed for the
    // scenes that have none.
    rt.setDebugMode(RayTracer::DebugMode::LIT);
    s.setEnvironment(nullptr);
    CHECK(traced(rt, s, eye, up) == Vec3Df(0.1f, 0.2f, 0.3f));
    // A copy of the scene shares the map; clearing the scene lets go of it.
    s.setEnvironment(world);
    const Scene copy = s;
    CHECK(copy.getEnvironment() == world);
    s.clear();
    CHECK(s.getEnvironment() == nullptr);
}

TEST_CASE("environment: a mirror sphere shows the map mirrored as predicted") {
    // Parallel rays along -Z meet the unit sphere where its normal makes the
    // angle a with +Z, and leave at 2a: the map read along (sin 2a, 0, cos 2a).
    const auto world = paintedWorld();
    REQUIRE(world != nullptr);
    const Scene s = sphereIn(world, mirror());
    RayTracer rt;
    const Vec3Df along(0.f, 0.f, -1.f);
    for (float degrees : {0.f, 10.f, 22.5f, 45.f, 60.f, 80.f, -30.f, -75.f}) {
        const float a = degrees * kPi / 180.f;
        const Vec3Df seen = traced(rt, s, Vec3Df(std::sin(a), 0.f, 5.f), along);
        checkColor(seen, world->sample(Vec3Df(std::sin(2.f * a), 0.f, std::cos(2.f * a))), 2e-4f,
                   "normal at " + std::to_string(degrees) + " degrees");
    }
    // Named: the middle of the ball shows what is behind the camera, its
    // right side at 45 degrees what is to the right, as a mirror does.
    checkColor(traced(rt, s, Vec3Df(0.f, 0.f, 5.f), along), world->sample(Vec3Df(0.f, 0.f, 1.f)), 2e-4f,
               "the centre looks back");
    const float h = std::sqrt(0.5f);
    checkColor(traced(rt, s, Vec3Df(h, 0.f, 5.f), along), world->sample(Vec3Df(1.f, 0.f, 0.f)), 2e-4f,
               "the right side looks right");
    // (Not at 45 degrees here: straight up is the pole, where every column
    // of a painted picture meets and the azimuth means nothing.)
    checkColor(traced(rt, s, Vec3Df(0.f, 0.5f, 5.f), along),
               world->sample(Vec3Df(0.f, std::sin(kPi / 3.f), std::cos(kPi / 3.f))), 2e-4f,
               "the upper part looks up");
    // Anywhere on the ball, not only along its equator: r = d - 2 (d.n) n.
    for (const Vec3Df& at : {Vec3Df(0.3f, 0.4f, 5.f), Vec3Df(-0.6f, 0.2f, 5.f), Vec3Df(0.1f, -0.7f, 5.f)}) {
        const Vec3Df n(at[0], at[1], std::sqrt(1.f - at[0] * at[0] - at[1] * at[1]));
        const Vec3Df r = along - 2.f * Vec3Df::dotProduct(along, n) * n;
        checkColor(traced(rt, s, at, along), world->sample(r), 2e-4f, "off the equator");
    }

    // The paper's own remark: with a map indexed by direction alone, every
    // point of the silhouette reflects "the environment diametrically
    // opposite the eye". At the rim the ray grazes and goes on, to -Z. Nine
    // texels, the middle one yellow: it is the one straight ahead, and the
    // whole rim of the ball is yellow, all the way around.
    const auto nine = worldOf("nine_texels.png", 3, 3,
                              {kRed, kGreen, kBlue, kMagenta, kYellow, kCyan, kWhite, kBlack, kRed});
    REQUIRE(nine != nullptr);
    const Scene ringed = sphereIn(nine, mirror());
    for (float around : {0.f, 1.f, 2.5f, 4.f, 5.5f}) {
        const float rim = std::sin(89.7f * kPi / 180.f);
        const Vec3Df seen = traced(rt, ringed, Vec3Df(rim * std::cos(around), rim * std::sin(around), 5.f), along);
        checkColor(seen, Vec3Df(1.f, 1.f, 0.f), 0.02f, "the silhouette at " + std::to_string(around) + " rad");
    }
    // ... while the middle of that same ball looks back, at the seam between
    // the left column and the right one.
    checkColor(traced(rt, ringed, Vec3Df(0.f, 0.f, 5.f), along), 0.5f * (Vec3Df(1.f, 0.f, 1.f) + Vec3Df(0.f, 1.f, 1.f)),
               1e-3f, "the centre of the nine-texel ball");

    // A half mirror keeps half of its own shading: with no light and no
    // ambient that is black, so half the world remains.
    rt.setAmbientIntensity(0.f);
    Scene half = sphereIn(world, Material(1.f, 0.f, Vec3Df(1.f, 1.f, 1.f), 32.f, 0.5f));
    checkColor(traced(rt, half, Vec3Df(h, 0.f, 5.f), along), 0.5f * world->sample(Vec3Df(1.f, 0.f, 0.f)), 2e-4f,
               "a half mirror");
    // And with no bounce allowed, the mirror shows nothing of the world.
    rt.setMaxDepth(0);
    checkColor(traced(rt, s, Vec3Df(h, 0.f, 5.f), along), Vec3Df(0.f, 0.f, 0.f), 0.f, "depth 0");
}

TEST_CASE("environment: glass of index 1 lets the world through untouched, and real glass bends it") {
    const auto world = paintedWorld();
    REQUIRE(world != nullptr);
    RayTracer rt;
    // No interface: no reflection, no bending. The ball is not there.
    const Scene air = sphereIn(world, glass(1.f));
    for (const Vec3Df& at : {Vec3Df(0.f, 0.f, 5.f), Vec3Df(0.5f, 0.2f, 5.f), Vec3Df(-0.3f, -0.8f, 5.f)}) {
        Vec3Df d = Vec3Df(0.1f, 0.05f, -1.f);
        d.normalize();
        checkColor(traced(rt, air, at, d), world->sample(d), 1e-5f, "through index 1");
    }

    // Through its centre a glass ball bends nothing either, but each face
    // sends 4 % of the light back: the world behind it comes through dimmed,
    // mixed with a little of what the faces reflect.
    const Scene ball = sphereIn(world, glass(1.5f));
    const Vec3Df along(0.f, 0.f, -1.f);
    const Vec3Df through = traced(rt, ball, Vec3Df(0.f, 0.f, 5.f), along);
    const Vec3Df behind = world->sample(along), back = world->sample(Vec3Df(0.f, 0.f, 1.f));
    // 0.96 x 0.96 straight through, 0.04 off the front face, and what little
    // bounces inside (0.96 x 0.04 x 0.96 back out of the front, then less).
    checkColor(through, 0.96f * 0.96f * behind + (0.04f + 0.96f * 0.04f * 0.96f) * back, 5e-3f, "through the centre");
    // Off-centre the ray is bent: it no longer sees what lies straight behind.
    const Vec3Df bent = traced(rt, ball, Vec3Df(0.6f, 0.f, 5.f), along);
    CHECK(Vec3Df::distance(bent, behind) > 0.05f);
}

TEST_CASE("environment: a Radiance map keeps what is brighter than white, a PNG is decoded from sRGB") {
    // One texel 50 times brighter than white, the sun of a small sky.
    HdrImage sky(4, 2);
    sky.fill(Vec3Df(0.25f, 0.25f, 0.25f));
    sky.set(2, 0, Vec3Df(50.f, 40.f, 30.f));  // third column, top row: 45 degrees right, 45 up
    const std::string path = test::outputDir() + "/small_sky.hdr";
    REQUIRE(sky.save(path));
    const auto world = Environment::load(path);
    REQUIRE(world != nullptr);
    const Vec3Df toSun(std::sin(kPi / 4.f) * std::sin(kPi / 4.f), std::cos(kPi / 4.f),
                       -std::sin(kPi / 4.f) * std::cos(kPi / 4.f));
    // RGBE keeps 8 bits of mantissa: within half a percent.
    checkColor(world->sample(toSun), Vec3Df(50.f, 40.f, 30.f), 0.25f, "the sun");
    // Radiance is linear already: 0.25 is 0.25, where a PNG's byte is decoded.
    checkColor(world->sample(Vec3Df(-1.f, -1.f, 0.f)), Vec3Df(0.25f, 0.25f, 0.25f), 2e-3f, "the sky, as stored");
    const Texel grey{{188, 188, 188}};
    const auto painted = worldOf("grey_world.png", 2, 1, {grey, grey});
    REQUIRE(painted != nullptr);
    CHECK_CLOSE(painted->sample(Vec3Df(0.f, 0.f, -1.f))[0], 0.5f, 4e-3);  // 188 is linear 0.5, not 0.737

    // The tracer passes that radiance on, above 1 included, to the display.
    Scene s;
    s.setEnvironment(world);
    RayTracer rt;
    const Camera cam = Camera::lookAt(Vec3Df(0.f, 0.f, 0.f), toSun, Vec3Df(0.f, 1.f, 0.f), kPi / 8.f, 1.f);
    const HdrImage img = rt.renderHdr(s, cam, 9, 9);
    CHECK_MSG(img.get(4, 4)[0] > 40.f, "the centre pixel looks at the sun: " + std::to_string(img.get(4, 4)[0]));
    CHECK_EQ(rt.getLastStats().hits, 0ul);

    // A file that is not there is a warning, not a crash.
    CHECK(Environment::load(test::outputDir() + "/no-such-world.hdr") == nullptr);
}

TEST_CASE("environment: tiles and threads see the same world") {
    const auto world = paintedWorld();
    REQUIRE(world != nullptr);
    Scene s = sphereIn(world, mirror());
    s.addObject(Object(std::make_shared<Sphere>(Vec3Df(1.6f, 0.2f, -0.5f), 0.6f), glass(1.5f)));
    RayTracer rt;
    rt.setAntiAliasing(2, true);
    const Camera cam = Camera::lookAt(Vec3Df(0.f, 0.5f, 5.f), Vec3Df(0.4f, 0.f, 0.f), Vec3Df(0.f, 1.f, 0.f), kPi / 4.f, 1.5f);
    const Image reference = rt.render(s, cam, 60, 40);
    const RayTracer::Stats referenceStats = rt.getLastStats();
    for (unsigned int threads : {1u, 3u}) {
        RenderJob job(rt, s, cam, 60, 40, 16, Vec3Df(0.f, 0.f, 0.f), threads);
        job.start();
        job.wait();
        CHECK_MSG(samePixels(rt.getDisplay().apply(job.hdrSnapshot()), reference),
                  std::to_string(threads) + " thread(s)");
        CHECK_EQ(job.stats().rays, referenceStats.rays);
        CHECK_EQ(job.stats().hits, referenceStats.hits);
    }
}
