// Bump mapping: J. F. Blinn, "Simulation of Wrinkled Surfaces", SIGGRAPH 1978.
// A height map tilts the normal light is computed with, and nothing else.
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "Bump.h"
#include "Camera.h"
#include "Environment.h"
#include "Fixtures.h"
#include "HdrImage.h"
#include "Image.h"
#include "Light.h"
#include "Material.h"
#include "ObjLoader.h"
#include "Primitive.h"
#include "RayTracer.h"
#include "RenderJob.h"
#include "Scene.h"
#include "Test.h"
#include "Texture.h"

namespace {

const float kPi = 3.14159265358979f;

/// A quad in the plane z = 0 covering [-half, half]^2, normal +Z, with the
/// unit square of coordinates spread over it: u along +X, v along +Y, or u
/// running the other way when `mirrored`.
Mesh texturedQuad(float half, bool mirrored = false) {
    const Vec3Df n(0.f, 0.f, 1.f);
    const float u0 = mirrored ? 1.f : 0.f, u1 = 1.f - u0;
    std::vector<Vertex> v = {
        Vertex(Vec3Df(-half, -half, 0.f), n, u0, 0.f), Vertex(Vec3Df(half, -half, 0.f), n, u1, 0.f),
        Vertex(Vec3Df(half, half, 0.f), n, u1, 1.f),   Vertex(Vec3Df(-half, half, 0.f), n, u0, 1.f)};
    return Mesh(v, {Triangle(0, 1, 2), Triangle(0, 2, 3)});
}

/// Height = u over the picture (texel centres sit on the line exactly), or
/// height = v. One period only: the map wraps, so its edges are a cliff.
std::shared_ptr<const Texture> ramp(bool alongV = false, int size = 64) {
    std::vector<Vec3Df> texels;
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x) {
            const float h = alongV ? 1.f - (static_cast<float>(y) + 0.5f) / static_cast<float>(size)
                                   : (static_cast<float>(x) + 0.5f) / static_cast<float>(size);
            texels.push_back(Vec3Df(h, h, h));
        }
    return Texture::fromTexels(size, size, texels);
}

std::shared_ptr<const Texture> flat(float height, int size = 8) {
    return Texture::fromTexels(size, size, std::vector<Vec3Df>(static_cast<size_t>(size) * size,
                                                                Vec3Df(height, height, height)));
}

Material bumped(const std::shared_ptr<const Texture>& heights, float scale, Material material = fixtures::white()) {
    material.setBumpMap(heights);
    material.setBumpScale(scale);
    return material;
}

/// The shading normal where a ray falling straight down meets the scene.
Vec3Df normalUnder(const RayTracer& rt, const Scene& scene, float x, float y) {
    RayTracer::Hit hit;
    REQUIRE(rt.closestHit(scene, Ray(Vec3Df(x, y, 5.f), Vec3Df(0.f, 0.f, -1.f)), hit));
    return rt.shadingNormal(scene, hit);
}

void checkVector(const Vec3Df& actual, const Vec3Df& expected, float tolerance, const std::string& what) {
    for (int c = 0; c < 3; ++c)
        CHECK_MSG(std::fabs(actual[c] - expected[c]) <= tolerance,
                  what + ", component " + std::to_string(c) + ": " + std::to_string(actual[c]) + " instead of " +
                      std::to_string(expected[c]));
}

bool sameFloats(const HdrImage& a, const HdrImage& b) {
    return a.width() == b.width() && a.height() == b.height() &&
           std::memcmp(a.data(), b.data(), static_cast<size_t>(a.width()) * a.height() * 3 * sizeof(float)) == 0;
}

/// A scene with a little of everything a normal feeds: the textured quad,
/// half matte with a highlight and half mirror, under the default rig, with
/// a cube standing on it to be reflected and to cast a shadow.
Scene busyScene(const Material& quadMaterial) {
    Scene s;
    s.addObject(Object(texturedQuad(2.f), quadMaterial));
    s.addObject(Object(fixtures::cube(Vec3Df(0.3f, -0.2f, 0.5f), 0.8f), Scene::defaultMaterial()));
    s.addDefaultLights();
    return s;
}

Material shinyHalfMirror() { return Material(0.8f, 0.6f, Vec3Df(0.9f, 0.8f, 0.7f), 32.f, 0.5f); }

/// A world where no two directions look alike, 32 x 16 texels.
std::shared_ptr<const Environment> paintedWorld() {
    const int w = 32, h = 16;
    std::vector<Vec3Df> texels;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            texels.push_back(Vec3Df(0.1f + 0.9f * static_cast<float>(x) / (w - 1),
                                    0.1f + 0.9f * static_cast<float>(y) / (h - 1),
                                    0.6f + 0.4f * std::sin(0.7f * static_cast<float>(x + 2 * y))));
    return std::make_shared<const Environment>(Texture::fromTexels(w, h, texels));
}

}  // namespace

TEST_CASE("bump: a triangle's tangents are what one unit of u and of v moves on the surface") {
    // The unit square over a quad 6 wide and 6 tall: one unit of u is 6 along X.
    const Mesh quad = texturedQuad(3.f);
    const std::vector<Vertex>& v = quad.getVertices();
    Vec3Df pu, pv;
    for (const Triangle& t : quad.getTriangles()) {
        REQUIRE(bump::tangents(v[t.getVertex(0)], v[t.getVertex(1)], v[t.getVertex(2)], pu, pv));
        checkVector(pu, Vec3Df(6.f, 0.f, 0.f), 1e-5f, "Pu");
        checkVector(pv, Vec3Df(0.f, 6.f, 0.f), 1e-5f, "Pv");
    }
    // Any triangle: moving from one corner to another by its own (du, dv)
    // along (Pu, Pv) must land on that corner.
    const Vertex a(Vec3Df(1.f, 2.f, 3.f), Vec3Df(0.f, 0.f, 1.f), 0.2f, 0.1f);
    const Vertex b(Vec3Df(4.f, 2.5f, 2.f), Vec3Df(0.f, 0.f, 1.f), 0.9f, 0.3f);
    const Vertex c(Vec3Df(2.f, 5.f, 3.5f), Vec3Df(0.f, 0.f, 1.f), 0.4f, 0.8f);
    REQUIRE(bump::tangents(a, b, c, pu, pv));
    checkVector(a.getPos() + (b.getU() - a.getU()) * pu + (b.getV() - a.getV()) * pv, b.getPos(), 1e-4f, "to b");
    checkVector(a.getPos() + (c.getU() - a.getU()) * pu + (c.getV() - a.getV()) * pv, c.getPos(), 1e-4f, "to c");

    // No coordinates (an OFF model: 0 everywhere), or all on one line: nothing to derive.
    const Mesh bare = fixtures::quad(0.f, 1.f);
    CHECK(!bump::tangents(bare.getVertices()[0], bare.getVertices()[1], bare.getVertices()[2], pu, pv));
    const Vertex d(Vec3Df(0.f, 0.f, 0.f), Vec3Df(0.f, 0.f, 1.f), 0.1f, 0.1f);
    const Vertex e(Vec3Df(1.f, 0.f, 0.f), Vec3Df(0.f, 0.f, 1.f), 0.2f, 0.2f);
    const Vertex f(Vec3Df(0.f, 1.f, 0.f), Vec3Df(0.f, 0.f, 1.f), 0.3f, 0.3f);
    CHECK(!bump::tangents(d, e, f, pu, pv));
}

TEST_CASE("bump: Blinn's normal is the normal of the surface pushed out by the height") {
    // On a plane with a height that is linear in (u, v) the first order is
    // the whole story: the displaced surface is the plane through
    // Pu + Fu n and Pv + Fv n, and its normal is their cross product.
    const Vec3Df pu(2.f, 0.3f, -0.4f), pv(0.5f, 1.5f, 0.2f);  // neither unit nor orthogonal
    Vec3Df n = Vec3Df::crossProduct(pu, pv);
    n.normalize();
    for (const auto& slopes : {std::pair<float, float>{0.7f, 0.f}, {0.f, -1.3f}, {0.4f, 0.9f}, {-2.5f, 0.1f}}) {
        const float fu = slopes.first, fv = slopes.second;
        Vec3Df expected = Vec3Df::crossProduct(pu + fu * n, pv + fv * n);
        expected.normalize();
        const Vec3Df got = bump::perturb(n, pu, pv, fu, fv);
        checkVector(got, expected, 1e-5f, "slopes " + std::to_string(fu) + ", " + std::to_string(fv));
        CHECK_CLOSE(got.getLength(), 1.f, 1e-5);

        // Coordinates that run mirrored over the surface: the outward normal
        // is -Pu x Pv, the height still pushes outward, along it.
        const Vec3Df out = -n;
        Vec3Df mirrored = Vec3Df::crossProduct(pv + fv * out, pu + fu * out);
        mirrored.normalize();
        checkVector(bump::perturb(out, pu, pv, fu, fv), mirrored, 1e-5f, "mirrored coordinates");
    }

    // A slope s, in world units of height per world unit along the surface,
    // tilts the normal by atan(s), away from the rise.
    for (float s : {0.1f, 0.5f, 1.f, 3.f}) {
        const Vec3Df tilted = bump::perturb(Vec3Df(0.f, 0.f, 1.f), Vec3Df(4.f, 0.f, 0.f), Vec3Df(0.f, 4.f, 0.f),
                                            4.f * s, 0.f);  // 4 s per unit of u, a unit of u being 4 wide
        CHECK_CLOSE(std::acos(tilted[2]), std::atan(s), 1e-5);
        CHECK(tilted[0] < 0.f);
        CHECK_CLOSE(tilted[1], 0.f, 1e-6);
    }

    // No slope: the very same normal, not one bit moved.
    const Vec3Df untouched = bump::perturb(n, pu, pv, 0.f, 0.f);
    CHECK(untouched[0] == n[0] && untouched[1] == n[1] && untouched[2] == n[2]);
}

TEST_CASE("bump: a height map's slopes are its neighbours' differences, and a data texture is not decoded") {
    float du = 0.f, dv = 0.f;
    // Height = u: a slope of 1 along u and none along v, wherever one asks
    // (away from the edge, where a wrapping ramp falls off its cliff).
    const auto alongU = ramp();
    for (float u : {0.1f, 0.33f, 0.5f, 0.9f})
        for (float v : {0.05f, 0.5f, 0.77f}) {
            alongU->heightGradient(u, v, du, dv);
            CHECK_CLOSE(du, 1.f, 1e-4);
            CHECK_CLOSE(dv, 0.f, 1e-6);
        }
    // Height = v, v growing upward as OBJ means it.
    ramp(true)->heightGradient(0.4f, 0.6f, du, dv);
    CHECK_CLOSE(du, 0.f, 1e-6);
    CHECK_CLOSE(dv, 1.f, 1e-4);
    // Flat: exactly nothing.
    flat(0.37f)->heightGradient(0.123f, 0.456f, du, dv);
    CHECK(du == 0.f && dv == 0.f);

    // From a file, a height map is numbers: byte / 255, where a colour
    // texture would decode sRGB and bend the ramp.
    Image image(256, 2, Image::RGB888);
    for (int x = 0; x < 256; ++x)
        for (int y = 0; y < 2; ++y) image.setPixel(x, y, static_cast<unsigned char>(x), static_cast<unsigned char>(x),
                                                   static_cast<unsigned char>(x));
    const std::string path = test::outputDir() + "/height_ramp.png";
    REQUIRE(image.save(path));
    const auto heights = Texture::readData(path);
    REQUIRE(heights != nullptr);
    CHECK_CLOSE(heights->sample((128.f + 0.5f) / 256.f, 0.5f)[0], 128.f / 255.f, 1e-6);
    CHECK_CLOSE(Texture::load(path)->sample((128.f + 0.5f) / 256.f, 0.5f)[0], 0.2158f, 1e-3);  // the same byte as a colour
    heights->heightGradient(0.5f, 0.5f, du, dv);
    CHECK_CLOSE(du, 256.f / 255.f, 1e-4);  // one byte per texel, 256 texels per unit of u
    CHECK(Texture::readData(test::outputDir() + "/no-such-heights.png") == nullptr);
}

TEST_CASE("bump: a ramp tilts the normal by atan(slope)") {
    RayTracer rt;
    // The ramp climbs `scale` over one unit of u, which is the quad's width.
    struct Case {
        float half, scale;
    };
    for (const Case& c : {Case{1.f, 0.5f}, Case{1.f, 2.f}, Case{2.f, 2.f}, Case{0.5f, 0.1f}, Case{1.f, 6.f}}) {
        const float slope = c.scale / (2.f * c.half), angle = std::atan(slope);
        const Scene s = fixtures::sceneOf(texturedQuad(c.half), bumped(ramp(), c.scale));
        const std::string what = "half " + std::to_string(c.half) + ", scale " + std::to_string(c.scale);
        for (float x : {-0.6f, 0.f, 0.45f})
            for (float y : {-0.5f, 0.2f}) {
                const Vec3Df n = normalUnder(rt, s, x * c.half, y * c.half);
                // Climbing toward +X, so the normal leans back toward -X.
                checkVector(n, Vec3Df(-std::sin(angle), 0.f, std::cos(angle)), 1e-4f, what);
                CHECK_CLOSE(std::acos(n[2]), angle, 1e-4);
            }
    }

    // A ramp along v leans it toward -Y; coordinates that run the other way
    // put the rise on the other side, and the lean with it.
    const float angle = std::atan(0.5f);
    checkVector(normalUnder(rt, fixtures::sceneOf(texturedQuad(1.f), bumped(ramp(true), 1.f)), 0.1f, 0.2f),
                Vec3Df(0.f, -std::sin(angle), std::cos(angle)), 1e-4f, "ramp along v");
    checkVector(normalUnder(rt, fixtures::sceneOf(texturedQuad(1.f, true), bumped(ramp(), 1.f)), 0.1f, 0.2f),
                Vec3Df(std::sin(angle), 0.f, std::cos(angle)), 1e-4f, "mirrored coordinates");
    // A negative scale digs where the map rises.
    checkVector(normalUnder(rt, fixtures::sceneOf(texturedQuad(1.f), bumped(ramp(), -1.f)), 0.1f, 0.2f),
                Vec3Df(std::sin(angle), 0.f, std::cos(angle)), 1e-4f, "negative scale");

    // The Normals mode paints that normal: (n + 1) / 2.
    rt.setDebugMode(RayTracer::DebugMode::NORMALS);
    RayTracer::Stats stats;
    const Scene s = fixtures::sceneOf(texturedQuad(1.f), bumped(ramp(), 1.f));
    const Vec3Df painted = rt.trace(s, Ray(Vec3Df(0.f, 0.f, 5.f), Vec3Df(0.f, 0.f, -1.f)), stats);
    checkVector(painted, 0.5f * (Vec3Df(-std::sin(angle), 0.f, std::cos(angle)) + Vec3Df(1.f, 1.f, 1.f)), 1e-4f,
                "normals mode");
}

TEST_CASE("bump: a flat height map changes no pixel") {
    const Camera cam = Camera::lookAt(Vec3Df(1.5f, -3.f, 3.f), Vec3Df(0.f, 0.f, 0.2f), Vec3Df(0.f, 0.f, 1.f),
                                      kPi / 4.f, 1.f);
    RayTracer rt;
    rt.setShadowSamples(2);
    rt.setAmbientOcclusion(2, 1.f);
    for (RayTracer::DebugMode mode : {RayTracer::DebugMode::LIT, RayTracer::DebugMode::NORMALS}) {
        rt.setDebugMode(mode);
        const std::string name = RayTracer::info(mode).slug;
        const HdrImage plain = rt.renderHdr(busyScene(shinyHalfMirror()), cam, 48, 48);
        // Flat, at any height and any scale: not a float differs.
        CHECK_MSG(sameFloats(rt.renderHdr(busyScene(bumped(flat(0.37f), 5.f, shinyHalfMirror())), cam, 48, 48), plain),
                  name + ", flat map");
        // A map with relief, weighed at nothing.
        CHECK_MSG(sameFloats(rt.renderHdr(busyScene(bumped(ramp(), 0.f, shinyHalfMirror())), cam, 48, 48), plain),
                  name + ", scale 0");
        // With relief the picture does change, or this test proves nothing...
        const Scene wrinkled = busyScene(bumped(ramp(), 1.f, shinyHalfMirror()));
        CHECK_MSG(!sameFloats(rt.renderHdr(wrinkled, cam, 48, 48), plain), name + ", a ramp shows");
        // ... and the switch brings the smooth surface back, bit for bit.
        rt.setBumpMapping(false);
        CHECK_MSG(sameFloats(rt.renderHdr(wrinkled, cam, 48, 48), plain), name + ", bump mapping off");
        rt.setBumpMapping(true);
    }

    // The surface itself has not moved: what measures it does not see the map.
    const Scene smooth = busyScene(shinyHalfMirror()), wrinkled = busyScene(bumped(ramp(), 1.f, shinyHalfMirror()));
    for (RayTracer::DebugMode mode :
         {RayTracer::DebugMode::HIT_MASK, RayTracer::DebugMode::DEPTH, RayTracer::DebugMode::AMBIENT_OCCLUSION,
          RayTracer::DebugMode::UV, RayTracer::DebugMode::AMBIENT, RayTracer::DebugMode::OBJECT_ID}) {
        rt.setDebugMode(mode);
        CHECK_MSG(sameFloats(rt.renderHdr(wrinkled, cam, 48, 48), rt.renderHdr(smooth, cam, 48, 48)),
                  RayTracer::info(mode).slug);
    }
}

TEST_CASE("bump: without coordinates, and on a primitive, the normal is the surface's own") {
    RayTracer rt;
    // An OFF model reads (0, 0) everywhere: no tangents, no tilt.
    const Scene bare = fixtures::sceneOf(fixtures::quad(0.f, 1.f), bumped(ramp(), 3.f));
    CHECK(normalUnder(rt, bare, 0.2f, -0.3f) == Vec3Df(0.f, 0.f, 1.f));
    // A sphere carries no coordinates either.
    Scene ball;
    ball.addObject(Object(std::make_shared<Sphere>(Vec3Df(0.f, 0.f, 0.f), 1.f), bumped(ramp(), 3.f)));
    RayTracer::Hit hit;
    REQUIRE(rt.closestHit(ball, Ray(Vec3Df(0.3f, 0.2f, 5.f), Vec3Df(0.f, 0.f, -1.f)), hit));
    CHECK(rt.shadingNormal(ball, hit) == hit.vertex.getNormal());
}

TEST_CASE("bump: Lambert and the highlight follow the tilted normal") {
    // A white matte quad, no ambient, one far light: the radiance is n . l.
    const float angle = std::atan(0.5f);  // scale 1 over a quad 2 wide
    const Vec3Df tilted(-std::sin(angle), 0.f, std::cos(angle));
    RayTracer rt;
    rt.setAmbientIntensity(0.f);
    RayTracer::Stats stats;
    const Ray down(Vec3Df(0.f, 0.f, 5.f), Vec3Df(0.f, 0.f, -1.f));
    auto litFrom = [&](const Vec3Df& direction, const Material& material) {
        Scene s = fixtures::sceneOf(texturedQuad(1.f), material);
        s.addLight(Light(1000.f * direction, Vec3Df(1.f, 1.f, 1.f), 1.f, 0.f));
        return rt.trace(s, down, stats)[0];
    };
    const Material smooth = fixtures::white(), wrinkled = bumped(ramp(), 1.f);
    // Lit from straight above, the slope receives cos(angle) of what the flat quad does.
    CHECK_CLOSE(litFrom(Vec3Df(0.f, 0.f, 1.f), smooth), 1.f, 1e-4);
    CHECK_CLOSE(litFrom(Vec3Df(0.f, 0.f, 1.f), wrinkled), std::cos(angle), 1e-4);
    // Lit along its own normal, the slope is fully lit and the flat quad is not.
    CHECK_CLOSE(litFrom(tilted, wrinkled), 1.f, 1e-4);
    CHECK_CLOSE(litFrom(tilted, smooth), std::cos(angle), 1e-4);
    // A light the tilted normal has turned its back on: black, though the
    // surface itself faces it.
    const Vec3Df behind(std::cos(angle) + 0.2f, 0.f, std::sin(angle));  // just past the slope's horizon
    CHECK(litFrom(behind, smooth) > 0.3f);
    CHECK_CLOSE(litFrom(behind, wrinkled), 0.f, 1e-6);

    // A light under the ground is under it for every bump, even one that
    // leans its way, and even with no shadow ray to say so.
    const Vec3Df below(-std::cos(0.1f), 0.f, -std::sin(0.1f));
    REQUIRE(Vec3Df::dotProduct(tilted, below) > 0.f);
    rt.setShadows(false);
    CHECK_CLOSE(litFrom(below, wrinkled), 0.f, 1e-6);
    rt.setShadows(true);

    // The highlight peaks where the half vector meets the tilted normal: the
    // eye is straight above, so the light sits at twice the tilt.
    Material shiny(0.f, 1.f, Vec3Df(1.f, 1.f, 1.f), 200.f);
    const Vec3Df mirrorOfEye(-std::sin(2.f * angle), 0.f, std::cos(2.f * angle));
    CHECK_CLOSE(litFrom(mirrorOfEye, bumped(ramp(), 1.f, shiny)), 1.f, 1e-3);
    CHECK(litFrom(mirrorOfEye, shiny) < 1e-3f);  // cos(angle)^200 on the flat quad
}

TEST_CASE("bump: a mirror reflects about the tilted normal, and never under the surface") {
    // A perfect mirror in a painted world: the colour of a ray tells where
    // it left for.
    const std::shared_ptr<const Environment> world = paintedWorld();
    RayTracer rt;
    rt.setAmbientIntensity(0.f);
    RayTracer::Stats stats;
    const Ray down(Vec3Df(0.f, 0.f, 5.f), Vec3Df(0.f, 0.f, -1.f));
    const Material mirror(1.f, 0.f, Vec3Df(1.f, 1.f, 1.f), 32.f, 1.f);
    auto seen = [&](float scale) {
        Scene s = fixtures::sceneOf(texturedQuad(1.f), bumped(ramp(), scale, mirror));
        s.setEnvironment(world);
        return rt.trace(s, down, stats);
    };

    // Flat: straight back up.
    checkVector(seen(0.f), world->sample(Vec3Df(0.f, 0.f, 1.f)), 1e-5f, "flat mirror");
    // A normal tilted by a turns the reflection by 2a.
    for (float scale : {0.4f, 1.f, 1.6f}) {
        const float a = std::atan(scale / 2.f);
        checkVector(seen(scale), world->sample(Vec3Df(-std::sin(2.f * a), 0.f, std::cos(2.f * a))), 1e-4f,
                    "scale " + std::to_string(scale));
    }
    // Past 45 degrees the mirrored ray points into the ground. A real
    // wrinkle would have caught it; here it is folded back above the
    // surface, instead of meeting the mirror again until the bounces run out
    // (which would leave the mirror's own colour: black).
    for (float scale : {3.f, 8.f}) {
        const float a = std::atan(scale / 2.f);
        REQUIRE(std::cos(2.f * a) < 0.f);
        checkVector(seen(scale), world->sample(Vec3Df(-std::sin(2.f * a), 0.f, -std::cos(2.f * a))), 1e-4f,
                    "steep, scale " + std::to_string(scale));
    }
}

TEST_CASE("bump: glass bends about the tilted normal") {
    // A pane of index 1 bends nothing whatever its normals: the world shows
    // through untouched. With a real index, the ray leaves along Snell's
    // direction about the tilted normal, where the flat pane goes straight.
    const std::shared_ptr<const Environment> world = paintedWorld();
    RayTracer rt;
    rt.setAmbientIntensity(0.f);
    RayTracer::Stats stats;
    const Ray down(Vec3Df(0.f, 0.f, 5.f), Vec3Df(0.f, 0.f, -1.f));
    auto seen = [&](float scale, float ior) {
        Material glass(1.f, 0.f, Vec3Df(1.f, 1.f, 1.f));
        glass.setTransparency(1.f);
        glass.setIor(ior);
        Scene s = fixtures::sceneOf(texturedQuad(1.f), bumped(ramp(), scale, glass));
        s.setEnvironment(world);
        return rt.trace(s, down, stats);
    };
    checkVector(seen(1.f, 1.f), world->sample(Vec3Df(0.f, 0.f, -1.f)), 1e-5f, "index 1");

    // One interface (the quad is a sheet, not a slab): air into glass.
    const float a = std::atan(0.5f), ior = 1.5f;
    const float bent = a - std::asin(std::sin(a) / ior);  // the ray turns toward the normal by i - t
    const Vec3Df through(std::sin(bent), 0.f, -std::cos(bent));
    const Vec3Df mirrored(-std::sin(2.f * a), 0.f, std::cos(2.f * a));
    const float cosi = std::cos(a), cost = std::cos(a - bent);
    const float rs = (cosi - ior * cost) / (cosi + ior * cost), rp = (ior * cosi - cost) / (ior * cosi + cost);
    const float f = 0.5f * (rs * rs + rp * rp);
    checkVector(seen(1.f, ior), f * world->sample(mirrored) + (1.f - f) * world->sample(through), 1e-4f,
                "glass of index 1.5 on a slope");
}

TEST_CASE("bump: tiles and threads give the same picture") {
    Scene s = busyScene(bumped(ramp(), 1.f, shinyHalfMirror()));
    const Camera cam = Camera::lookAt(Vec3Df(1.5f, -3.f, 3.f), Vec3Df(0.f, 0.f, 0.2f), Vec3Df(0.f, 0.f, 1.f),
                                      kPi / 4.f, 1.5f);
    RayTracer rt;
    rt.setAntiAliasing(2, true);
    rt.setShadowSamples(2);
    const HdrImage reference = rt.renderHdr(s, cam, 60, 40);
    const RayTracer::Stats referenceStats = rt.getLastStats();
    for (unsigned int threads : {1u, 3u}) {
        RenderJob job(rt, s, cam, 60, 40, 16, Vec3Df(0.f, 0.f, 0.f), threads);
        job.start();
        job.wait();
        CHECK_MSG(sameFloats(job.hdrSnapshot(), reference), std::to_string(threads) + " thread(s)");
        CHECK_EQ(job.stats().rays, referenceStats.rays);
        CHECK_EQ(job.stats().hits, referenceStats.hits);
    }
}

TEST_CASE("obj: MTL map_bump loads the height map as data, and -bm is its scale") {
    Image image(4, 4, Image::RGB888);
    image.fill(128, 128, 128);
    REQUIRE(image.save(test::outputDir() + "/grey_heights.png"));
    fixtures::writeFile("bumpy.mtl",
        "newmtl scaled\nKd 1 1 1\nmap_bump -bm 0.25 grey_heights.png\n"
        "newmtl plain\nKd 1 1 1\nbump grey_heights.png\n"
        "newmtl lost\nKd 1 1 1\nmap_Bump -bm 2 nowhere.png\n"
        "newmtl smooth\nKd 1 1 1\n");
    const std::string obj = fixtures::writeFile("bumpy.obj",
        "mtllib bumpy.mtl\n"
        "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\nvn 0 0 1\n"
        "usemtl scaled\nf 1/1/1 2/2/1 3/3/1\n"
        "usemtl plain\nf 1/1/1 3/3/1 4/4/1\n"
        "usemtl lost\nf 1/1/1 2/2/1 4/4/1\n"
        "usemtl smooth\nf 2/2/1 3/3/1 4/4/1\n");
    const std::vector<Object> objects = loadOBJ(obj, fixtures::white());
    REQUIRE(objects.size() == 4);
    const Material& scaled = objects[0].getMaterial();
    REQUIRE(scaled.getBumpMap() != nullptr);
    CHECK_CLOSE(scaled.getBumpScale(), 0.25f, 1e-7);
    // 128 as a number, 0.502, not as a colour, 0.216.
    CHECK_CLOSE(scaled.getBumpMap()->sample(0.5f, 0.5f)[0], 128.f / 255.f, 1e-6);
    CHECK(scaled.getDiffuseMap() == nullptr);

    const Material& plain = objects[1].getMaterial();
    REQUIRE(plain.getBumpMap() != nullptr);
    CHECK_CLOSE(plain.getBumpScale(), 1.f, 1e-7);  // -bm defaults to 1
    // A map that points nowhere leaves the surface smooth; so does no map.
    CHECK(objects[2].getMaterial().getBumpMap() == nullptr);
    CHECK(objects[3].getMaterial().getBumpMap() == nullptr);
}
