// Tinted glass and the shadows of glass: D. S. Kay & D. Greenberg,
// "Transparency for Computer Synthesized Images", SIGGRAPH 1979. What gets
// through falls off with the thickness crossed, and a shadow ray is filtered
// by glass instead of stopped.
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "Camera.h"
#include "Environment.h"
#include "Fixtures.h"
#include "HdrImage.h"
#include "Light.h"
#include "Material.h"
#include "ObjLoader.h"
#include "Optics.h"
#include "Primitive.h"
#include "RayTracer.h"
#include "RenderJob.h"
#include "Sampler.h"
#include "Scene.h"
#include "Test.h"
#include "Texture.h"

namespace {

const float kPi = 3.14159265358979f;
const Vec3Df kWhite(1.f, 1.f, 1.f), kBlack(0.f, 0.f, 0.f), kUp(0.f, 0.f, 1.f), kDown(0.f, 0.f, -1.f);

Material glass(float ior, const Vec3Df& absorption = kBlack, float transparency = 1.f) {
    Material m(1.f, 0.f, kWhite);
    m.setTransparency(transparency);
    m.setIor(ior);
    m.setAbsorption(absorption);
    return m;
}

/// A slab of glass lying flat: [-half, half]^2 across, from z = 1 up to 1 + thickness.
Object slab(const Material& material, float thickness = 1.f, float half = 6.f) {
    return Object(fixtures::box(Vec3Df(-half, -half, 1.f), Vec3Df(half, half, 1.f + thickness)), material);
}

/// A white matte floor z = 0, facing up.
Object floorQuad() { return Object(fixtures::quad(0.f, 20.f), fixtures::white()); }

void checkColor(const Vec3Df& actual, const Vec3Df& expected, float tolerance, const std::string& what) {
    for (int c = 0; c < 3; ++c)
        CHECK_MSG(std::fabs(actual[c] - expected[c]) <= tolerance,
                  what + ", channel " + std::to_string(c) + ": " + std::to_string(actual[c]) + " instead of " +
                      std::to_string(expected[c]));
}

Vec3Df squared(const Vec3Df& v) { return v * v; }

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

/// What a ray falling straight down from z = 5 at (x, 0) sees: nothing lit,
/// so only the world, through and off whatever stands in the scene.
Vec3Df seenFromAbove(const RayTracer& rt, Scene scene, const std::shared_ptr<const Environment>& world,
                     const Vec3Df& direction = kDown) {
    scene.setEnvironment(world);
    RayTracer::Stats stats;
    return rt.trace(scene, Ray(Vec3Df(0.f, 0.f, 5.f), direction), stats);
}

/// Share of a far light straight above (or along `toLight`) that reaches the
/// floor at the origin, under whatever stands in the scene.
Vec3Df reachingFloor(const RayTracer& rt, Scene scene, const Vec3Df& toLight = kUp) {
    scene.addObject(floorQuad());
    Vec3Df l = toLight;
    l.normalize();
    scene.addLight(Light(1000.f * l, kWhite, 1.f, 0.f));
    Sampler sampler;
    return rt.lightVisibility(scene, Vec3Df(0.f, 0.f, 0.f), kUp, scene.getLights()[0], sampler);
}

Scene sceneOf(const Object& object) {
    Scene s;
    s.addObject(object);
    return s;
}

bool sameFloats(const HdrImage& a, const HdrImage& b) {
    return a.width() == b.width() && a.height() == b.height() &&
           std::memcmp(a.data(), b.data(), static_cast<size_t>(a.width()) * a.height() * 3 * sizeof(float)) == 0;
}

}  // namespace

TEST_CASE("absorption: Beer-Lambert, and the absorption that gives a tint at a depth") {
    const Vec3Df sigma(0.5f, 1.f, 2.f);
    checkColor(optics::transmittance(sigma, 0.f), kWhite, 0.f, "no distance");
    checkColor(optics::transmittance(sigma, 1.f), Vec3Df(std::exp(-0.5f), std::exp(-1.f), std::exp(-2.f)), 1e-6f, "d = 1");
    // Twice the thickness passes the square of the share.
    checkColor(optics::transmittance(sigma, 2.f), squared(optics::transmittance(sigma, 1.f)), 1e-6f, "d = 2");

    // The other way round: the absorption that leaves `tint` after `depth`.
    const Vec3Df tint(0.5f, 0.25f, 1.f);
    const Vec3Df a = Material::absorptionFor(tint, 2.f);
    checkColor(optics::transmittance(a, 2.f), tint, 1e-6f, "the tint at its depth");
    checkColor(optics::transmittance(a, 4.f), squared(tint), 1e-6f, "its square at twice the depth");
    CHECK(a[2] == 0.f);  // a channel that passes everything absorbs nothing
    // Black would take an infinite absorption: held finite.
    CHECK(std::isfinite(Material::absorptionFor(kBlack, 1.f)[0]));
    CHECK(Material::absorptionFor(kBlack, 1.f)[0] > 5.f);

    Material m;
    CHECK(!m.absorbs());  // clear by default
    m.setAbsorption(Vec3Df(-1.f, 0.f, 0.3f));
    CHECK(m.getAbsorption() == Vec3Df(0.f, 0.f, 0.3f));  // glass does not amplify
    CHECK(m.absorbs());
}

TEST_CASE("absorption: a slab of absorption sigma and thickness d passes exp(-sigma d)") {
    // Index 1: nothing bends and nothing reflects, so the slab only absorbs.
    const std::shared_ptr<const Environment> world = paintedWorld();
    const Vec3Df behind = world->sample(kDown);
    RayTracer rt;
    rt.setAmbientIntensity(0.f);
    const Vec3Df sigma(0.5f, 1.f, 2.f);
    checkColor(seenFromAbove(rt, sceneOf(slab(glass(1.f))), world), behind, 1e-6f, "clear");
    for (float thickness : {0.25f, 1.f, 2.f})
        checkColor(seenFromAbove(rt, sceneOf(slab(glass(1.f, sigma), thickness)), world),
                   behind * optics::transmittance(sigma, thickness), 1e-5f, "thickness " + std::to_string(thickness));
    // Crossed at an angle, the path inside is thickness / cos: more is lost.
    // To 5e-4 only: the ray inside starts a hair under the face it entered
    // by (the bias that keeps it from meeting that face again), which moves
    // its way out sideways by as much.
    for (float degrees : {30.f, 45.f}) {
        const float a = degrees * kPi / 180.f;
        const Vec3Df slanted(std::sin(a), 0.f, -std::cos(a));
        checkColor(seenFromAbove(rt, sceneOf(slab(glass(1.f, sigma))), world, slanted),
                   world->sample(slanted) * optics::transmittance(sigma, 1.f / std::cos(a)), 5e-4f,
                   std::to_string(degrees) + " degrees");
    }
    // Half glass: the other half is the surface's own shading, black here,
    // at each of the two faces.
    checkColor(seenFromAbove(rt, sceneOf(slab(glass(1.f, sigma, 0.5f))), world),
               0.25f * behind * optics::transmittance(sigma, 1.f), 1e-5f, "transparency 0.5");
}

TEST_CASE("absorption: light mirrored inside the glass is absorbed along every leg") {
    // Real glass, met head-on. Each face reflects F = 0.04 and each crossing
    // of the slab keeps a = exp(-sigma d). Down comes (1 - F)^2 a, then the
    // same after two more legs inside, F^2 a^2 of it, and so on; back up,
    // F from the top face, then (1 - F)^2 F a^2 from the bottom one, and so
    // on. Per channel, since a is a colour.
    const std::shared_ptr<const Environment> world = paintedWorld();
    RayTracer rt;
    rt.setAmbientIntensity(0.f);
    const Vec3Df sigma(0.2f, 0.7f, 1.5f);
    const Vec3Df seen = seenFromAbove(rt, sceneOf(slab(glass(1.5f, sigma))), world);
    const float f = 0.04f;
    Vec3Df expected;
    for (int c = 0; c < 3; ++c) {
        const float a = std::exp(-sigma[c]);
        const float echo = 1.f / (1.f - f * f * a * a);
        const float through = (1.f - f) * (1.f - f) * a * echo;
        const float back = f + (1.f - f) * (1.f - f) * f * a * a * echo;
        expected[c] = through * world->sample(kDown)[c] + back * world->sample(kUp)[c];
    }
    checkColor(seen, expected, 1e-4f, "tinted slab of index 1.5");
}

TEST_CASE("absorption: a ray that enters an open sheet and meets nothing is not absorbed") {
    // One quad is a face with nothing behind it to leave by: no thickness
    // was crossed, so no thickness is charged.
    const std::shared_ptr<const Environment> world = paintedWorld();
    RayTracer rt;
    rt.setAmbientIntensity(0.f);
    const Scene sheet = sceneOf(Object(fixtures::quad(1.f, 6.f), glass(1.f, Vec3Df(3.f, 3.f, 3.f))));
    checkColor(seenFromAbove(rt, sheet, world), world->sample(kDown), 1e-6f, "open sheet");
}

TEST_CASE("glass shadows: a pane of index 1 casts no shadow") {
    RayTracer rt;
    const Vec3Df through = reachingFloor(rt, sceneOf(slab(glass(1.f))));
    CHECK(through == kWhite);  // exactly: nothing reflected, nothing absorbed
    // As if it were not there, to the last float of a lit floor.
    Scene with = sceneOf(slab(glass(1.f))), without;
    for (Scene* s : {&with, &without}) {
        s->addObject(floorQuad());
        s->addLight(Light(Vec3Df(300.f, 200.f, 1000.f), Vec3Df(1.f, 0.8f, 0.6f), 0.9f, 40.f));
    }
    rt.setShadowSamples(3);
    RayTracer::Stats stats;
    // A ray that reaches the floor under the pane without crossing it.
    const Ray under(Vec3Df(12.f, 0.f, 0.5f), Vec3Df(-12.f, 0.f, -0.5f) / Vec3Df(-12.f, 0.f, -0.5f).getLength());
    const Vec3Df a = rt.trace(with, under, stats), b = rt.trace(without, under, stats);
    CHECK(a[0] == b[0] && a[1] == b[1] && a[2] == b[2]);
    CHECK(a[0] > 0.5f);
}

TEST_CASE("glass shadows: clear glass at normal incidence passes (1 - 0.04)^2") {
    RayTracer rt;
    const float f = 0.04f;  // ((1.5 - 1) / (1.5 + 1))^2
    checkColor(reachingFloor(rt, sceneOf(slab(glass(1.5f)))), (1.f - f) * (1.f - f) * kWhite, 1e-6f, "index 1.5");
    // Another index, another share: water.
    const float fw = (0.33f / 2.33f) * (0.33f / 2.33f);
    checkColor(reachingFloor(rt, sceneOf(slab(glass(1.33f)))), (1.f - fw) * (1.f - fw) * kWhite, 1e-6f, "index 1.33");
    // At an angle each face reflects more, the same share on the way in and
    // on the way out of a slab.
    const float a = 60.f * kPi / 180.f;
    const float fa = optics::fresnel(std::cos(a), 1.f, 1.5f);
    CHECK(fa > 0.08f);
    checkColor(reachingFloor(rt, sceneOf(slab(glass(1.5f))), Vec3Df(std::sin(a), 0.f, std::cos(a))),
               (1.f - fa) * (1.f - fa) * kWhite, 1e-5f, "60 degrees");

    // On the floor itself: a white matte floor under a white light straight
    // above shows exactly what reached it.
    Scene s = sceneOf(slab(glass(1.5f)));
    s.addObject(floorQuad());
    s.addLight(Light(Vec3Df(0.f, 0.f, 1000.f), kWhite, 1.f, 0.f));
    rt.setAmbientIntensity(0.f);
    RayTracer::Stats stats;
    const Vec3Df toOrigin = Vec3Df(-12.f, 0.f, -0.5f) / Vec3Df(-12.f, 0.f, -0.5f).getLength();
    checkColor(rt.trace(s, Ray(Vec3Df(12.f, 0.f, 0.5f), toOrigin), stats), 0.9216f * kWhite, 1e-4f, "the floor");
}

TEST_CASE("glass shadows: tinted glass casts the shadow of its colour, deeper where it is thicker") {
    RayTracer rt;
    const Vec3Df sigma(0.1f, 0.8f, 2.f);  // lets red through
    checkColor(reachingFloor(rt, sceneOf(slab(glass(1.f, sigma)))), optics::transmittance(sigma, 1.f), 1e-5f, "d = 1");
    checkColor(reachingFloor(rt, sceneOf(slab(glass(1.f, sigma), 2.f))), optics::transmittance(sigma, 2.f), 1e-5f,
               "d = 2");
    // With the two faces of real glass on top.
    checkColor(reachingFloor(rt, sceneOf(slab(glass(1.5f, sigma)))), 0.9216f * optics::transmittance(sigma, 1.f), 1e-5f,
               "index 1.5");
    // A slanted light crosses thickness / cos of it (shadow rays are not bent).
    const float a = 45.f * kPi / 180.f;
    checkColor(reachingFloor(rt, sceneOf(slab(glass(1.f, sigma))), Vec3Df(std::sin(a), 0.f, std::cos(a))),
               optics::transmittance(sigma, 1.f / std::cos(a)), 1e-5f, "45 degrees");
    // Two slabs one above the other: the product.
    Scene two = sceneOf(slab(glass(1.f, sigma)));
    two.addObject(Object(fixtures::box(Vec3Df(-6.f, -6.f, 3.f), Vec3Df(6.f, 6.f, 3.5f)), glass(1.f, Vec3Df(1.f, 0.f, 0.f))));
    checkColor(reachingFloor(rt, two), optics::transmittance(sigma, 1.f) * Vec3Df(std::exp(-0.5f), 1.f, 1.f), 1e-5f,
               "two slabs");

    // A glass ball, an equation and not a mesh: under its centre the light
    // crossed a diameter and two faces head-on.
    Scene ball;
    ball.addObject(Object(std::make_shared<Sphere>(Vec3Df(0.f, 0.f, 3.f), 1.5f), glass(1.5f, sigma)));
    checkColor(reachingFloor(rt, ball), 0.9216f * optics::transmittance(sigma, 3.f), 1e-4f, "sphere");

    // The highlight is tinted like the rest: the light arrives coloured.
    Scene s = sceneOf(slab(glass(1.f, sigma)));
    s.addObject(Object(fixtures::quad(0.f, 20.f), Material(0.f, 1.f, kWhite, 50.f)));
    s.addLight(Light(Vec3Df(0.f, 0.f, 1000.f), kWhite, 1.f, 0.f));
    rt.setAmbientIntensity(0.f);
    RayTracer::Stats stats;
    // Seen from where the half vector is nearly the normal: almost straight above, outside the slab's reach.
    const Vec3Df highlight = rt.trace(s, Ray(Vec3Df(0.f, 0.f, 0.9f), kDown), stats);
    checkColor(highlight, optics::transmittance(sigma, 1.f), 1e-4f, "highlight under tinted glass");
}

TEST_CASE("glass shadows: what is opaque still blocks, and half glass passes half at each face") {
    RayTracer rt;
    CHECK(reachingFloor(rt, sceneOf(slab(fixtures::white()))) == kBlack);
    // Opaque above the glass, or under it: black either way.
    for (float z : {0.4f, 4.f}) {
        Scene s = sceneOf(slab(glass(1.f)));
        s.addObject(Object(fixtures::box(Vec3Df(-3.f, -3.f, z), Vec3Df(3.f, 3.f, z + 0.2f)), fixtures::white()));
        CHECK_MSG(reachingFloor(rt, s) == kBlack, "an opaque box at z = " + std::to_string(z));
    }
    // Transparency 0.5: half of each face is opaque.
    checkColor(reachingFloor(rt, sceneOf(slab(glass(1.f, kBlack, 0.5f)))), 0.25f * kWhite, 1e-6f, "half glass");
    // An open sheet has one face, whichever side the light meets it from,
    // and no thickness to absorb in.
    const Vec3Df sigma(3.f, 3.f, 3.f);
    checkColor(reachingFloor(rt, sceneOf(Object(fixtures::quad(2.f, 6.f), glass(1.5f, sigma)))), 0.96f * kWhite, 1e-6f,
               "sheet facing the light");
    Mesh facingDown = fixtures::quad(2.f, 6.f);
    std::vector<Triangle> flipped;
    for (const Triangle& t : facingDown.getTriangles())
        flipped.push_back(Triangle(t.getVertex(0), t.getVertex(2), t.getVertex(1)));
    checkColor(reachingFloor(rt, sceneOf(Object(Mesh(facingDown.getVertices(), flipped), glass(1.5f, sigma)))),
               0.96f * kWhite, 1e-6f, "sheet facing away");

    // The switch: glass is opaque to shadow rays again, as it was.
    rt.setTransparentShadows(false);
    CHECK(reachingFloor(rt, sceneOf(slab(glass(1.f)))) == kBlack);
    CHECK(reachingFloor(rt, sceneOf(slab(glass(1.5f, Vec3Df(0.1f, 0.8f, 2.f))))) == kBlack);
    // ... and a point nothing covers is fully lit either way.
    Scene aside = sceneOf(Object(fixtures::box(Vec3Df(3.f, 3.f, 1.f), Vec3Df(4.f, 4.f, 2.f)), glass(1.5f)));
    CHECK(reachingFloor(rt, aside) == kWhite);
    rt.setTransparentShadows(true);
    CHECK(reachingFloor(rt, aside) == kWhite);
}

TEST_CASE("glass shadows: a scene without glass renders the same floats with the switch on or off") {
    Scene s;
    s.addObject(Object(fixtures::cube(Vec3Df(0.f, 0.f, 0.6f), 1.f), Scene::defaultMaterial()));
    s.addDefaultLights();
    s.addObject(floorQuad());
    const Camera cam = Camera::lookAt(Vec3Df(2.f, -4.f, 3.f), Vec3Df(0.f, 0.f, 0.3f), kUp, kPi / 4.f, 1.f);
    RayTracer rt;
    rt.setShadowSamples(3);
    rt.setAmbientOcclusion(2, 1.f);
    const HdrImage on = rt.renderHdr(s, cam, 48, 48);
    rt.setTransparentShadows(false);
    CHECK(sameFloats(rt.renderHdr(s, cam, 48, 48), on));
}

TEST_CASE("glass shadows: soft shadows average what each ray lets through, the same on any tiles and threads") {
    // A tinted glass cube on a floor, under an area light: penumbra and tint.
    Scene s;
    s.addObject(Object(fixtures::box(Vec3Df(-0.5f, -0.5f, 0.f), Vec3Df(0.5f, 0.5f, 1.f)),
                       glass(1.5f, Vec3Df(0.2f, 1.f, 2.5f))));
    s.addDefaultLights();
    s.addObject(floorQuad());
    const Camera cam = Camera::lookAt(Vec3Df(2.f, -4.f, 3.f), Vec3Df(0.f, 0.f, 0.3f), kUp, kPi / 4.f, 1.5f);
    RayTracer rt;
    rt.setAntiAliasing(2, true);
    rt.setShadowSamples(3);
    const HdrImage reference = rt.renderHdr(s, cam, 60, 40);
    const RayTracer::Stats referenceStats = rt.getLastStats();
    for (unsigned int threads : {1u, 3u}) {
        RenderJob job(rt, s, cam, 60, 40, 16, kBlack, threads);
        job.start();
        job.wait();
        CHECK_MSG(sameFloats(job.hdrSnapshot(), reference), std::to_string(threads) + " thread(s)");
        CHECK_EQ(job.stats().rays, referenceStats.rays);
        CHECK_EQ(job.stats().hits, referenceStats.hits);
    }
    // The brute-force path walks the same faces.
    rt.setBvhEnabled(false);
    CHECK(sameFloats(rt.renderHdr(s, cam, 60, 40), reference));
    rt.setBvhEnabled(true);

    // Every sample of a small light far above a wide slab crosses it almost
    // head-on: the soft shadow is the hard one.
    Scene wide = sceneOf(slab(glass(1.5f)));
    wide.addObject(floorQuad());
    wide.addLight(Light(Vec3Df(0.f, 0.f, 1000.f), kWhite, 1.f, 5.f));
    rt.setShadowSamples(4);
    Sampler sampler;
    checkColor(rt.lightVisibility(wide, Vec3Df(0.f, 0.f, 0.f), kUp, wide.getLights()[0], sampler), 0.9216f * kWhite,
               1e-4f, "a small area light");
}

TEST_CASE("obj: MTL Tf is what a unit of length inside the material lets through") {
    fixtures::writeFile("tinted.mtl", "newmtl bottle\nKd 1 1 1\nd 0.1\nNi 1.5\nTf 0.2 0.8 0.3\nnewmtl clear\nKd 1 1 1\nd 0.1\n");
    const std::string obj = fixtures::writeFile("tinted.obj",
        "mtllib tinted.mtl\nv 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nvn 0 0 1\n"
        "usemtl bottle\nf 1//1 2//1 3//1\nusemtl clear\nf 1//1 3//1 4//1\n");
    const std::vector<Object> objects = loadOBJ(obj, fixtures::white());
    REQUIRE(objects.size() == 2);
    const Material& bottle = objects[0].getMaterial();
    CHECK_CLOSE(bottle.getTransparency(), 0.9f, 1e-6);
    checkColor(optics::transmittance(bottle.getAbsorption(), 1.f), Vec3Df(0.2f, 0.8f, 0.3f), 1e-6f, "Tf at one unit");
    CHECK(!objects[1].getMaterial().absorbs());
}
