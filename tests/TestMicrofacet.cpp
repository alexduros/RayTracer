// The Cook-Torrance highlight: R. L. Cook & K. E. Torrance, "A Reflectance
// Model for Computer Graphics", SIGGRAPH 1981. Facets of Beckmann slopes,
// the share of them their neighbours hide, and the Fresnel reflectance.
#include <cmath>
#include <cstring>
#include <random>
#include <string>

#include "Camera.h"
#include "Fixtures.h"
#include "HdrImage.h"
#include "Light.h"
#include "Material.h"
#include "Microfacet.h"
#include "Optics.h"
#include "RayTracer.h"
#include "RenderJob.h"
#include "Scene.h"
#include "Test.h"

namespace {

const float kPi = 3.14159265358979f;
const Vec3Df kUp(0.f, 0.f, 1.f), kWhite(1.f, 1.f, 1.f);

/// Unit direction at `theta` from +Z, turned by `phi` about it.
Vec3Df polar(float theta, float phi = 0.f) {
    return Vec3Df(std::sin(theta) * std::cos(phi), std::sin(theta) * std::sin(phi), std::cos(theta));
}

/// Integral over the hemisphere of f(l, v) (n.l) dl, the share of the light
/// coming from v the surface sends back anywhere. Taken over facet normals
/// h, where Beckmann's D lives, with dl = 4 (v.h) dh: then it is the
/// integral of D G F (v.h) / (n.v) dh.
float albedo(const Vec3Df& v, float m, float ior) {
    const int nTheta = 4000, nPhi = 256;
    const float dTheta = 0.5f * kPi / nTheta, dPhi = 2.f * kPi / nPhi;
    double sum = 0.0;
    for (int i = 0; i < nTheta; ++i) {
        const float t = (static_cast<float>(i) + 0.5f) * dTheta;
        for (int j = 0; j < nPhi; ++j) {
            const Vec3Df h = polar(t, (static_cast<float>(j) + 0.5f) * dPhi);
            const float vDotH = Vec3Df::dotProduct(v, h);
            if (vDotH <= 0.f) continue;
            const Vec3Df l = optics::reflect(-1.f * v, h);
            const float nDotL = l[2], nDotV = v[2], nDotH = h[2];
            if (nDotL <= 0.f) continue;
            const float term = microfacet::beckmann(nDotH, m) * microfacet::geometry(nDotH, nDotV, nDotL, vDotH) *
                               optics::fresnel(vDotH, 1.f, ior) * vDotH / nDotV;
            sum += static_cast<double>(term) * std::sin(t) * dTheta * dPhi;
        }
    }
    return static_cast<float>(sum);
}

/// What the eye along `v` sees of a quad z = 0 (white, all highlight, no
/// diffuse) lit by one far white light along `l`, under Cook-Torrance.
Vec3Df highlightSeen(const Material& material, const Vec3Df& v, const Vec3Df& l) {
    Scene s = fixtures::sceneOf(fixtures::quad(0.f, 50.f), material);
    s.addLight(Light(1e5f * l, kWhite, 1.f, 0.f));
    RayTracer rt;
    rt.setAmbientIntensity(0.f);
    rt.setSpecularModel(RayTracer::SpecularModel::COOK_TORRANCE);
    RayTracer::Stats stats;
    return rt.trace(s, Ray(10.f * v, -1.f * v), stats);
}

Material shiny(float m, bool metal = false, const Vec3Df& color = kWhite) {
    Material mat(0.f, 1.f, color);
    mat.setRoughness(m);
    mat.setMetal(metal);
    return mat;
}

bool sameFloats(const HdrImage& a, const HdrImage& b) {
    return a.width() == b.width() && a.height() == b.height() &&
           std::memcmp(a.data(), b.data(), static_cast<size_t>(a.width()) * a.height() * 3 * sizeof(float)) == 0;
}

}  // namespace

TEST_CASE("microfacet: Beckmann's facets cover the surface exactly once, at any roughness") {
    // The integral of D(h) (n.h) over the hemisphere: the facets, projected
    // onto the surface, tile it.
    for (float m : {0.05f, 0.2f, 0.5f, 1.f}) {
        const int n = 200000;
        const double dTheta = 0.5 * kPi / n;
        double sum = 0.0;
        for (int i = 0; i < n; ++i) {
            const double t = (i + 0.5) * dTheta;
            sum += microfacet::beckmann(static_cast<float>(std::cos(t)), m) * std::cos(t) * std::sin(t) * 2.0 * kPi *
                   dTheta;
        }
        CHECK_MSG(std::fabs(sum - 1.0) < 1e-3, "m = " + std::to_string(m) + ": " + std::to_string(sum));
    }
    // Smoother is narrower and taller: head-on, D = 1 / (pi m^2).
    CHECK_CLOSE(microfacet::beckmann(1.f, 0.1f), 1.f / (kPi * 0.01f), 1e-2);
    CHECK(microfacet::beckmann(std::cos(0.3f), 0.1f) < microfacet::beckmann(std::cos(0.3f), 0.3f));
    CHECK(microfacet::beckmann(0.f, 0.5f) == 0.f);
}

TEST_CASE("microfacet: the attenuation hides facets only toward grazing, alike for the eye and the light") {
    // Head-on nothing is hidden.
    CHECK(microfacet::geometry(1.f, 1.f, 1.f, 1.f) == 1.f);
    // Symmetric in the eye and the light.
    for (float nh : {0.7f, 0.95f})
        for (float nv : {0.1f, 0.5f})
            for (float nl : {0.2f, 0.9f}) {
                const float vh = 0.6f;
                CHECK(microfacet::geometry(nh, nv, nl, vh) == microfacet::geometry(nh, nl, nv, vh));
                CHECK(microfacet::geometry(nh, nv, nl, vh) <= 1.f);
            }
    // Lower and lower as the eye goes down to the horizon.
    float last = 2.f;
    for (float degrees : {60.f, 75.f, 85.f, 89.f}) {
        const Vec3Df v = polar(degrees * kPi / 180.f), l = polar(0.f);
        Vec3Df h = v + l;
        h.normalize();
        const float g = microfacet::geometry(h[2], v[2], l[2], Vec3Df::dotProduct(v, h));
        CHECK_MSG(g < last, std::to_string(degrees) + " degrees: " + std::to_string(g));
        last = g;
    }
    CHECK(last < 0.1f);
}

TEST_CASE("microfacet: f(l, v) = f(v, l)") {
    std::minstd_rand random(7);
    auto uniform = [&random]() { return static_cast<float>(random() - random.min()) / (random.max() - random.min()); };
    const Vec3Df n = kUp;
    for (int k = 0; k < 500; ++k) {
        const Vec3Df l = polar(0.5f * kPi * uniform(), 2.f * kPi * uniform());
        const Vec3Df v = polar(0.5f * kPi * uniform(), 2.f * kPi * uniform());
        for (float m : {0.1f, 0.4f})
            for (float ior : {1.5f, 8.f}) {
                const float a = microfacet::reflectance(n, v, l, m, ior), b = microfacet::reflectance(n, l, v, m, ior);
                CHECK_MSG(std::fabs(a - b) <= 1e-5f * std::max(1.f, std::fabs(a)),
                          std::to_string(a) + " against " + std::to_string(b));
            }
    }
    // Under the surface, either way: nothing.
    CHECK(microfacet::reflectance(n, polar(0.3f), Vec3Df(0.f, 0.6f, -0.8f), 0.3f, 1.5f) == 0.f);
}

TEST_CASE("microfacet: a polished surface reflects what a mirror would, and never more than it receives") {
    // As m goes to 0 the highlight becomes the mirror's: all the light that
    // comes along v and leaves, the Fresnel share F of it. This is where the
    // 4 in the denominator is decided (the paper's pi would give 4 / pi of it).
    for (float degrees : {0.f, 40.f, 70.f}) {
        const Vec3Df v = polar(degrees * kPi / 180.f, 0.3f);
        CHECK_CLOSE(albedo(v, 0.02f, 1.5f), optics::fresnel(v[2], 1.f, 1.5f), 2e-3);
    }
    // A polished white metal returns nearly everything, a rough one loses
    // what its facets' shadows hide (this model does not bounce it again).
    const float whiteMetal = microfacet::iorFromReflectance(1.f);
    const float polished = albedo(polar(0.f), 0.05f, whiteMetal), rough = albedo(polar(0.f), 0.6f, whiteMetal);
    CHECK_MSG(polished > 0.97f && polished <= 1.0005f, std::to_string(polished));
    CHECK_MSG(rough < polished && rough > 0.7f, std::to_string(rough));  // about 0.8 at m = 0.6
    // Never more than it receives, from any angle.
    for (float degrees : {0.f, 30.f, 60.f, 80.f, 88.f})
        for (float m : {0.05f, 0.3f, 1.f}) {
            const float a = albedo(polar(degrees * kPi / 180.f), m, whiteMetal);
            CHECK_MSG(a <= 1.001f, std::to_string(degrees) + " degrees, m = " + std::to_string(m) + ": " +
                                       std::to_string(a));
        }
}

TEST_CASE("microfacet: plastic reflects more and more toward grazing") {
    // Eye and light mirrored about the normal, at an angle t: h = n, so
    // D = 1 / (pi m^2), G = 1, and the radiance seen is F(t) / (4 m^2 cos t)
    // x the light. It rises with t, through F and through 1 / cos t: what
    // Blinn-Phong's constant peak misses, and why a floor seen low shines.
    const float m = 0.1f;
    float last = 0.f;
    for (float degrees : {0.f, 30.f, 60.f, 75.f, 85.f}) {
        const float t = degrees * kPi / 180.f;
        const Vec3Df seen = highlightSeen(shiny(m), polar(t), polar(t, kPi));
        const float expected = optics::fresnel(std::cos(t), 1.f, 1.5f) / (4.f * m * m * std::cos(t));
        CHECK_MSG(std::fabs(seen[0] - expected) <= 2e-3f * expected,
                  std::to_string(degrees) + " degrees: " + std::to_string(seen[0]) + " instead of " +
                      std::to_string(expected));
        CHECK(seen[0] > last);
        last = seen[0];
    }
    // Head-on, 4 % of a mirror's, spread over a lobe of slope m.
    CHECK_CLOSE(highlightSeen(shiny(m), kUp, kUp)[0], 0.04f / (4.f * m * m), 1e-4);
    // Rougher: wider and dimmer at the peak.
    CHECK(highlightSeen(shiny(0.3f), kUp, kUp)[0] < highlightSeen(shiny(0.1f), kUp, kUp)[0]);
    CHECK(highlightSeen(shiny(0.3f), kUp, polar(0.4f))[0] > highlightSeen(shiny(0.1f), kUp, polar(0.4f))[0]);
}

TEST_CASE("microfacet: a metal shines in its own colour, whitens at grazing, and scatters nothing") {
    // Its colour is its reflectance head-on: the index Cook and Torrance
    // derive from it gives that reflectance back.
    for (float f0 : {0.f, 0.04f, 0.5f, 0.95f})
        CHECK_CLOSE(optics::fresnel(1.f, 1.f, microfacet::iorFromReflectance(f0)), f0, 1e-5);
    const Vec3Df copper(0.95f, 0.64f, 0.54f);
    const float m = 0.1f;
    const Vec3Df headOn = highlightSeen(shiny(m, true, copper), kUp, kUp);
    for (int c = 0; c < 3; ++c) CHECK_CLOSE(headOn[c], copper[c] / (4.f * m * m), 1e-3 * headOn[c]);
    // At grazing the channels close up toward white: 1.76 between red and
    // blue head-on, within 1.15 of each other two degrees from the horizon.
    // (A real index dips first, near its Brewster angle, then rises: copper
    // is redder at 75 degrees than head-on, as the Fresnel equations say.)
    const float t = 88.f * kPi / 180.f;
    const Vec3Df low = highlightSeen(shiny(m, true, copper), polar(t), polar(t, kPi));
    const float spread = std::max(low[0], std::max(low[1], low[2])) / std::min(low[0], std::min(low[1], low[2]));
    CHECK_MSG(spread < 1.15f, std::to_string(spread));
    // No diffuse light: lit far from the mirror direction, a polished metal
    // is black where plastic of the same colour would be lit.
    Material plastic(1.f, 0.f, copper), metal = shiny(0.05f, true, copper);
    metal.setDiffuse(1.f);
    for (const Material* mat : {&plastic, &metal}) {
        Scene s = fixtures::sceneOf(fixtures::quad(0.f, 50.f), *mat);
        s.addLight(Light(1e5f * polar(0.8f, kPi), kWhite, 1.f, 0.f));
        RayTracer rt;
        rt.setAmbientIntensity(0.f);
        rt.setSpecularModel(RayTracer::SpecularModel::COOK_TORRANCE);
        RayTracer::Stats stats;
        const Vec3Df seen = rt.trace(s, Ray(Vec3Df(0.f, 0.f, 10.f), Vec3Df(0.f, 0.f, -1.f)), stats);
        if (mat == &plastic)
            CHECK(seen[0] > 0.6f);
        else
            CHECK(seen[0] < 1e-6f);
    }
    // Under Blinn-Phong a metal is the same idea, cruder: its lobe in its colour, no diffuse.
    Scene s = fixtures::sceneOf(fixtures::quad(0.f, 50.f), shiny(0.1f, true, copper));
    s.addLight(Light(1e5f * kUp, kWhite, 1.f, 0.f));
    RayTracer rt;
    rt.setAmbientIntensity(0.f);
    RayTracer::Stats stats;
    const Vec3Df phong = rt.trace(s, Ray(Vec3Df(0.f, 0.f, 10.f), Vec3Df(0.f, 0.f, -1.f)), stats);
    for (int c = 0; c < 3; ++c) CHECK_CLOSE(phong[c], copper[c], 1e-5);
}

TEST_CASE("microfacet: roughness follows the shininess unless it is set") {
    Material m;
    m.setShininess(32.f);
    CHECK_CLOSE(m.getRoughness(), std::sqrt(2.f / 34.f), 1e-6);
    m.setRoughness(0.3f);
    CHECK_CLOSE(m.getRoughness(), 0.3f, 1e-7);
    m.setRoughness(-1.f);  // back to the shininess's
    CHECK_CLOSE(m.getRoughness(), std::sqrt(2.f / 34.f), 1e-6);
    // The two lobes then have the same width: at the angle where Blinn-Phong
    // falls to half its peak, Beckmann has fallen to about half its own.
    const float n = 32.f, rough = std::sqrt(2.f / (n + 2.f));
    const float half = std::acos(std::pow(0.5f, 1.f / n));
    const float ratio = microfacet::beckmann(std::cos(half), rough) / microfacet::beckmann(1.f, rough);
    CHECK_MSG(ratio > 0.4f && ratio < 0.6f, std::to_string(ratio));
}

TEST_CASE("microfacet: Blinn-Phong stays the default, and the model changes highlights only") {
    Scene s;
    s.addObject(Object(fixtures::cube(Vec3Df(0.f, 0.f, 0.6f), 1.f), Scene::defaultMaterial()));
    s.addDefaultLights();
    s.addObject(Object(fixtures::quad(0.f, 20.f), fixtures::white()));
    const Camera cam = Camera::lookAt(Vec3Df(2.f, -4.f, 3.f), Vec3Df(0.f, 0.f, 0.3f), kUp, kPi / 4.f, 1.5f);
    RayTracer rt;
    rt.setShadowSamples(2);
    const HdrImage byDefault = rt.renderHdr(s, cam, 48, 32);
    CHECK(rt.getSpecularModel() == RayTracer::SpecularModel::BLINN_PHONG);
    rt.setSpecularModel(RayTracer::SpecularModel::BLINN_PHONG);
    CHECK(sameFloats(rt.renderHdr(s, cam, 48, 32), byDefault));
    rt.setSpecularModel(RayTracer::SpecularModel::COOK_TORRANCE);
    const HdrImage microfacets = rt.renderHdr(s, cam, 48, 32);
    CHECK(!sameFloats(microfacets, byDefault));
    // Without highlights the two models are one picture.
    rt.setSpecularEnabled(false);
    const HdrImage plainCT = rt.renderHdr(s, cam, 48, 32);
    rt.setSpecularModel(RayTracer::SpecularModel::BLINN_PHONG);
    CHECK(sameFloats(rt.renderHdr(s, cam, 48, 32), plainCT));

    // Tiles and threads, with soft shadows, give the same floats.
    rt.setSpecularEnabled(true);
    rt.setSpecularModel(RayTracer::SpecularModel::COOK_TORRANCE);
    rt.setAntiAliasing(2, true);
    const HdrImage reference = rt.renderHdr(s, cam, 60, 40);
    for (unsigned int threads : {1u, 3u}) {
        RenderJob job(rt, s, cam, 60, 40, 16, Vec3Df(0.f, 0.f, 0.f), threads);
        job.start();
        job.wait();
        CHECK_MSG(sameFloats(job.hdrSnapshot(), reference), std::to_string(threads) + " thread(s)");
    }
}
