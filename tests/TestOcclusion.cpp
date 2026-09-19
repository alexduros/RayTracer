#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "Camera.h"
#include "Fixtures.h"
#include "Image.h"
#include "Light.h"
#include "Material.h"
#include "RayTracer.h"
#include "Sampler.h"
#include "Scene.h"
#include "Test.h"

namespace {

const float kPi = 3.14159265358979f;
const Vec3Df kWhiteColor(1.f, 1.f, 1.f);
const Vec3Df kUp(0.f, 0.f, 1.f);

// A floor z = 0 facing up and a wall x = 0 facing +X standing on it, both far
// larger than any radius used here: the inner corner of two quads.
Scene cornerScene() {
    Scene s;
    s.addObject(Object(fixtures::quad(0.f, 20.f), Material(1.f, 0.f, kWhiteColor)));
    const Vec3Df n(1.f, 0.f, 0.f);
    const std::vector<Vertex> v = {Vertex(Vec3Df(0.f, -20.f, 0.f), n), Vertex(Vec3Df(0.f, 20.f, 0.f), n),
                                   Vertex(Vec3Df(0.f, 20.f, 20.f), n), Vertex(Vec3Df(0.f, -20.f, 20.f), n)};
    s.addObject(Object(Mesh(v, {Triangle(0, 1, 2), Triangle(0, 2, 3)}), Material(1.f, 0.f, kWhiteColor)));
    return s;
}

/// Open fraction at floor point (x, 0, 0), sampled with a fresh sampler of
/// the given seed.
float openAt(const RayTracer& rt, const Scene& s, float x, unsigned int seed = 1u) {
    Sampler sampler(seed);
    return rt.ambientOcclusion(s, Vec3Df(x, 0.f, 0.f), kUp, sampler);
}

/// Share of a unit disk with x >= c (a circular segment's area over pi).
float diskShareBeyond(float c) {
    c = std::max(-1.f, std::min(1.f, c));
    return (std::acos(c) - c * std::sqrt(1.f - c * c)) / kPi;
}

/// Linear colour of the surface seen along the ray from `from` to `to`.
Vec3Df shadeAt(const RayTracer& rt, const Scene& s, const Vec3Df& from, const Vec3Df& to) {
    Vec3Df d = to - from;
    d.normalize();
    RayTracer::Stats st;
    return rt.trace(s, Ray(from, d), st);
}

}  // namespace

TEST_CASE("occlusion: off by default, and a lone plane is open everywhere") {
    Scene s = fixtures::sceneOf(fixtures::quad(0.f, 10.f));
    s.addLight(Light(Vec3Df(2.f, 3.f, 6.f), kWhiteColor, 1.f, 0.f));
    RayTracer rt;
    CHECK_EQ(rt.getAmbientOcclusionSamplesPerAxis(), 0u);
    rt.setAmbientOcclusion(8, 5.f);
    for (int k = -8; k <= 8; ++k) {
        Sampler sampler;
        const float x = 1.1f * static_cast<float>(k);
        CHECK_MSG(rt.ambientOcclusion(s, Vec3Df(x, 0.3f * x, 0.f), kUp, sampler) == 1.f,
                  "open plane at x = " + std::to_string(x));
    }
    // So a render with occlusion is the render without it, byte for byte.
    const Camera cam = Camera::lookAt(Vec3Df(0.f, 0.f, 5.f), Vec3Df(0.f, 0.f, 0.f), Vec3Df(0.f, 1.f, 0.f), kPi / 4.f, 1.f);
    const Image occluded = rt.render(s, cam, 32, 32);
    rt.setAmbientOcclusion(0, 5.f);
    const Image plain = rt.render(s, cam, 32, 32);
    REQUIRE(plain.sizeInBytes() == occluded.sizeInBytes());
    CHECK(std::memcmp(plain.data(), occluded.data(), plain.sizeInBytes()) == 0);
}

TEST_CASE("occlusion: next to a wall the open share is the disk beyond d / radius") {
    // Cosine-weighted directions are points of the unit disk lifted onto the
    // hemisphere. From a floor point at distance d from the wall, a direction
    // meets the wall within the radius R when its component toward the wall
    // exceeds d / R: that part of the disk is a circular segment, so the open
    // share is 1 - segment(d / R), 1/2 in the corner and 1 from d = R on.
    const Scene s = cornerScene();
    RayTracer rt;
    for (float radius : {1.f, 2.f}) {
        rt.setAmbientOcclusion(16, radius);
        float previous = 0.f;
        for (int k = 0; k <= 24; ++k) {
            const float d = 0.05f * radius * static_cast<float>(k) + 0.01f;
            const float open = openAt(rt, s, d);
            const float expected = 1.f - diskShareBeyond(d / radius);
            const std::string at = "radius " + std::to_string(radius) + ", d = " + std::to_string(d) + ", open " +
                                   std::to_string(open) + ", expected " + std::to_string(expected);
            CHECK_MSG(std::fabs(open - expected) < 0.03f, at);
            CHECK_MSG(open >= previous, "brighter away from the wall, " + at);
            if (d > 1.02f * radius) CHECK_MSG(open == 1.f, "beyond the radius, " + at);
            previous = open;
        }
    }
    // A wider radius sees the wall from farther: the same point gets darker.
    rt.setAmbientOcclusion(16, 1.f);
    const float near = openAt(rt, s, 0.5f);
    rt.setAmbientOcclusion(16, 4.f);
    CHECK(openAt(rt, s, 0.5f) < near - 0.2f);
}

TEST_CASE("occlusion: the estimate tightens as the samples grow") {
    // Same point, many seeds: the spread of the answers shrinks with n x n.
    const Scene s = cornerScene();
    RayTracer rt;
    auto spread = [&](unsigned int samplesPerAxis) {
        rt.setAmbientOcclusion(samplesPerAxis, 1.f);
        float sum = 0.f, sumSquares = 0.f;
        const int seeds = 64;
        for (int i = 0; i < seeds; ++i) {
            const float open = openAt(rt, s, 0.3f, 1000u + 7u * static_cast<unsigned int>(i));
            sum += open;
            sumSquares += open * open;
        }
        const float mean = sum / seeds;
        return std::sqrt(std::max(0.f, sumSquares / seeds - mean * mean));
    };
    const float coarse = spread(2), fine = spread(8);
    CHECK_MSG(coarse > 0.02f, "2x2 is noisy: " + std::to_string(coarse));
    CHECK_MSG(fine < 0.25f * coarse, "8x8 " + std::to_string(fine) + " vs 2x2 " + std::to_string(coarse));
    // And the same seed gives the same answer.
    rt.setAmbientOcclusion(4, 1.f);
    CHECK(openAt(rt, s, 0.3f, 42u) == openAt(rt, s, 0.3f, 42u));
}

TEST_CASE("occlusion: scales the ambient and diffuse terms, not the highlight") {
    // Floor point 0.3 from the wall, one light high above it, no shadow in
    // the way: with occlusion the colour is the open share times the colour
    // without it. trace's fixed samplers make the share reproducible.
    Scene s = cornerScene();
    s.addLight(Light(Vec3Df(3.f, 0.f, 10.f), kWhiteColor, 1.f, 0.f));
    const Vec3Df eye(3.f, 0.f, 4.f), point(0.3f, 0.f, 0.f);
    RayTracer rt;
    rt.setSpecularEnabled(false);
    const Vec3Df plain = shadeAt(rt, s, eye, point);
    rt.setAmbientOcclusion(8, 1.f);
    PixelSamplers probe;  // the samplers trace uses without a pixel
    const float open = rt.ambientOcclusion(s, point, kUp, probe.occlusion);
    REQUIRE(open > 0.6f && open < 0.95f);
    const Vec3Df occluded = shadeAt(rt, s, eye, point);
    CHECK_CLOSE(occluded[0], open * plain[0], 1e-5);

    // A purely specular floor (no ambient, no diffuse) keeps its highlight.
    for (Object& o : s.getObjects()) o.getMaterial() = Material(0.f, 1.f, kWhiteColor, 8.f);
    rt.setAmbientIntensity(0.f);
    rt.setSpecularEnabled(true);
    const Vec3Df highlight = shadeAt(rt, s, eye, point);
    rt.setAmbientOcclusion(0, 1.f);
    const Vec3Df reference = shadeAt(rt, s, eye, point);
    CHECK(highlight[0] > 0.01f);
    CHECK_CLOSE(highlight[0], reference[0], 0.f);
}
