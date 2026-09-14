// BVH (experiment 3): the tree may change the speed, never the picture. The
// brute-force scan stays the reference (RayTracer::setBvhEnabled (false)) and
// every check compares against it exactly: same object, same distance bit for
// bit, same interpolated vertex.
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <random>
#include <string>
#include <vector>

#include "Bvh.h"
#include "Camera.h"
#include "Fixtures.h"
#include "Orientation.h"
#include "RayTracer.h"
#include "Scene.h"
#include "Test.h"

namespace {

const float kPi = 3.14159265358979f;

/// Uniform in [0, 1) from the raw engine output, the same on every platform.
float uniform01(std::minstd_rand& rng) {
    return static_cast<float>(rng() - std::minstd_rand::min()) /
           (static_cast<float>(std::minstd_rand::max() - std::minstd_rand::min()) + 1.f);
}

Vec3Df randomDirection(std::minstd_rand& rng) {
    for (;;) {
        Vec3Df v(2.f * uniform01(rng) - 1.f, 2.f * uniform01(rng) - 1.f, 2.f * uniform01(rng) - 1.f);
        const float lengthSquared = v.getSquaredLength();
        if (lengthSquared > 1e-4f && lengthSquared <= 1.f) {
            v.normalize();
            return v;
        }
    }
}

/// Uniform point in `box` grown by `margin` x its size on every side.
Vec3Df randomPointIn(const BoundingBox& box, float margin, std::minstd_rand& rng) {
    const float grow = margin * box.getSize();
    Vec3Df p;
    for (int i = 0; i < 3; ++i) {
        const float lo = box.getMin()[i] - grow, hi = box.getMax()[i] + grow;
        p[i] = lo + (hi - lo) * uniform01(rng);
    }
    return p;
}

/// A bundled model set up as the CLI does: loaded, stood upright, lit,
/// optionally on its ground plane. The teapot and the ram are Z-up, so their
/// scenes also prove that Scene::setUpAxis rebuilds the tree.
Scene loadModel(const char* model, bool ground) {
    Scene s;
    s.addObjectsFromFile(test::modelPath(model));
    s.setUpAxis(resolveUpAxis(test::modelPath(model), s));
    s.addDefaultLights();
    if (ground) s.addGroundPlane();
    return s;
}

/// cells x cells squares of side `cell` in the plane z = 0, facing +Z: flat
/// leaf boxes whose borders are triangle edges.
Mesh flatGrid(unsigned int cells, float cell) {
    const Vec3Df n(0.f, 0.f, 1.f);
    std::vector<Vertex> v;
    std::vector<Triangle> t;
    for (unsigned int j = 0; j <= cells; ++j)
        for (unsigned int i = 0; i <= cells; ++i)
            v.push_back(Vertex(Vec3Df(cell * static_cast<float>(i), cell * static_cast<float>(j), 0.f), n));
    for (unsigned int j = 0; j < cells; ++j)
        for (unsigned int i = 0; i < cells; ++i) {
            const unsigned int a = j * (cells + 1) + i, b = a + 1, c = a + cells + 2, d = a + cells + 1;
            t.push_back(Triangle(a, b, c));
            t.push_back(Triangle(a, c, d));
        }
    return Mesh(v, t);
}

std::string describe(bool found, const RayTracer::Hit& h) {
    if (!found) return "miss";
    return "object " + std::to_string(h.objectIndex) + " at " + std::to_string(h.distance);
}

/// Every ray must hit exactly the same thing through the BVH as by brute force,
/// and the shadow-ray query (occluded) must agree with that closest hit on
/// both paths: nothing strictly before it, something as soon as the limit
/// passes it.
void compareRays(const std::string& name, const Scene& scene, const std::vector<Ray>& rays) {
    RayTracer withBvh, bruteForce;
    bruteForce.setBvhEnabled(false);
    const float kInfinity = std::numeric_limits<float>::infinity();
    size_t hits = 0, mismatches = 0;
    for (size_t k = 0; k < rays.size(); ++k) {
        RayTracer::Hit a, b;
        const bool foundA = withBvh.closestHit(scene, rays[k], a);
        const bool foundB = bruteForce.closestHit(scene, rays[k], b);
        if (foundB) ++hits;
        const bool same = foundA == foundB &&
                          (!foundA || (a.objectIndex == b.objectIndex && a.distance == b.distance &&
                                       a.vertex.getPos() == b.vertex.getPos() &&
                                       a.vertex.getNormal() == b.vertex.getNormal()));
        if (!same && ++mismatches <= 3)
            CHECK_MSG(false, name + " ray " + std::to_string(k) + ": bvh " + describe(foundA, a) + ", brute force " +
                                 describe(foundB, b));

        const float closest = foundB ? b.distance : std::numeric_limits<float>::max();
        bool occlusionAgrees = true;
        for (const RayTracer* rt : {&withBvh, &bruteForce})
            occlusionAgrees = occlusionAgrees && !rt->occluded(scene, rays[k], closest) &&
                              rt->occluded(scene, rays[k], std::nextafter(closest, kInfinity)) == foundB;
        if (!occlusionAgrees && ++mismatches <= 3)
            CHECK_MSG(false, name + " ray " + std::to_string(k) + ": occluded() disagrees with the closest hit, " +
                                 describe(foundB, b));
    }
    CHECK_MSG(mismatches == 0,
              name + ": " + std::to_string(mismatches) + " of " + std::to_string(rays.size()) + " rays differ");
    // Enough rays must hit for the comparison to mean something.
    CHECK_MSG(hits > rays.size() / 5,
              name + ": only " + std::to_string(hits) + " of " + std::to_string(rays.size()) + " rays hit");
}

/// `count` random rays around `box`, cycling through four kinds: from outside
/// toward the model (like a primary ray), from inside the box in any direction
/// (like a shadow ray), and both again along an exact axis with +0 or -0 on
/// the other two components (1 / 0 = +-inf in the slab test).
std::vector<Ray> randomRays(const BoundingBox& box, unsigned int count) {
    const float size = box.getSize();
    std::minstd_rand rng(20260914u);
    std::vector<Ray> rays;
    for (unsigned int k = 0; k < count; ++k) {
        const bool fromOutside = (k % 2) == 0;
        Vec3Df origin, direction;
        if (k % 4 < 2) {
            if (fromOutside) {
                origin = box.getCenter() + 1.5f * size * randomDirection(rng);
                direction = randomPointIn(box, 0.1f, rng) - origin;
                direction.normalize();
            } else {
                origin = randomPointIn(box, 0.f, rng);
                direction = randomDirection(rng);
            }
        } else {
            const int axis = static_cast<int>(rng() % 3);
            const float sign = (rng() % 2) ? 1.f : -1.f;
            for (int i = 0; i < 3; ++i) direction[i] = (rng() % 2) ? 0.f : -0.f;
            direction[axis] = sign;
            origin = randomPointIn(box, 0.1f, rng);
            if (fromOutside) origin[axis] -= sign * 2.f * size;
        }
        rays.push_back(Ray(origin, direction));
    }
    return rays;
}

/// Rays aimed exactly at every triangle corner and at points along every edge,
/// from oblique directions and along the axes. Those are the rays a leaf box
/// without margin loses: the triangle test accepts a hit on an edge that the
/// slab test, rounding the other way, places just outside the box.
std::vector<Ray> edgeRays(const Scene& scene) {
    const float size = std::max(scene.getBoundingBox().getSize(), 1e-3f);
    std::vector<Vec3Df> directions = {Vec3Df(-1.f, -2.f, -3.f), Vec3Df(2.f, -1.f, -1.5f), Vec3Df(0.3f, -1.f, 0.7f),
                                      Vec3Df(0.7f, 0.4f, -1.f), Vec3Df(1.f, 2.f, 3.f),    Vec3Df(-2.f, 1.f, 1.5f),
                                      Vec3Df(0.f, -1.f, 0.f),   Vec3Df(0.f, 0.f, -1.f),   Vec3Df(-1.f, 0.f, 0.f)};
    for (Vec3Df& d : directions) d.normalize();
    std::vector<Ray> rays;
    for (const Object& object : scene.getObjects()) {
        const std::vector<Vertex>& vertices = object.getMesh().getVertices();
        for (const Triangle& triangle : object.getMesh().getTriangles())
            for (unsigned int e = 0; e < 3; ++e) {
                const Vec3Df& a = vertices[triangle.getVertex(e)].getPos();
                const Vec3Df& b = vertices[triangle.getVertex((e + 1) % 3)].getPos();
                for (unsigned int s = 0; s < 4; ++s) {  // the corner, then 1/4, 1/2 and 3/4 along the edge
                    const Vec3Df target = a + (b - a) * (static_cast<float>(s) / 4.f);
                    for (const Vec3Df& d : directions) rays.push_back(Ray(target - (2.f * size) * d, d));
                }
            }
    }
    return rays;
}

}  // namespace

TEST_CASE("bvh: every triangle sits in exactly one leaf of at most four, inside nested boxes") {
    const Scene s = loadModel("ram", false);
    const Object& object = s.getObjects()[0];
    const Mesh& mesh = object.getMesh();
    const std::vector<Bvh::Node>& nodes = object.getBvh().getNodes();
    const std::vector<unsigned int>& order = object.getBvh().getTriangleOrder();
    const unsigned int n = static_cast<unsigned int>(mesh.getTriangles().size());
    REQUIRE(!nodes.empty());
    REQUIRE(order.size() == n);

    std::vector<unsigned int> seen(n, 0);
    for (unsigned int i : order) {
        REQUIRE(i < n);
        ++seen[i];
    }
    CHECK(std::all_of(seen.begin(), seen.end(), [](unsigned int c) { return c == 1; }));

    // Depth first, left child first: the leaves must tile `order` from 0 to n.
    struct Visit {
        unsigned int node, parent, depth;
    };
    std::vector<Visit> stack = {{0, 0, 1}};
    unsigned int nextTriangle = 0, leaves = 0, maxDepth = 0, outsideBoxes = 0;
    while (!stack.empty()) {
        const Visit v = stack.back();
        stack.pop_back();
        REQUIRE(v.node < nodes.size());
        const Bvh::Node& node = nodes[v.node];
        const BoundingBox& parent = nodes[v.parent].box;
        if (!parent.contains(node.box.getMin()) || !parent.contains(node.box.getMax())) ++outsideBoxes;
        maxDepth = std::max(maxDepth, v.depth);
        if (node.isLeaf()) {
            ++leaves;
            CHECK(node.count <= Bvh::kDefaultLeafSize);
            CHECK_EQ(node.index, nextTriangle);
            nextTriangle = node.index + node.count;
            for (unsigned int k = node.index; k < node.index + node.count; ++k)
                for (unsigned int corner = 0; corner < 3; ++corner) {
                    const Triangle& triangle = mesh.getTriangles()[order[k]];
                    if (!node.box.contains(mesh.getVertices()[triangle.getVertex(corner)].getPos())) ++outsideBoxes;
                }
        } else {
            REQUIRE(node.index > v.node + 1 && node.index < nodes.size());
            stack.push_back({node.index, v.node, v.depth + 1});  // right, after the whole left subtree
            stack.push_back({v.node + 1, v.node, v.depth + 1});
        }
    }
    CHECK_EQ(nextTriangle, n);
    CHECK_EQ(outsideBoxes, 0u);
    CHECK_EQ(nodes.size(), 2u * leaves - 1u);  // a full binary tree, every node reached once
    // Median splits halve the count at every level.
    CHECK_MSG(maxDepth <= static_cast<unsigned int>(std::ceil(std::log2(double(n)))),
              "depth " + std::to_string(maxDepth) + " for " + std::to_string(n) + " triangles");
}

TEST_CASE("bvh: at equal distance the lowest triangle index wins, as in the brute-force scan") {
    // Sixteen coplanar triangles all covering (0.5, -1, 0), one per leaf. The
    // highest index has the smallest centroid, so it lands in the leftmost
    // leaf and is found first; the traversal must still return copy 0.
    std::vector<Vertex> v;
    std::vector<Triangle> t;
    for (unsigned int k = 0; k < 16; ++k) {
        const float s = 0.1f * static_cast<float>(15 - k);
        const Vec3Df n(0.01f * static_cast<float>(k), 0.f, 1.f);  // tells the copies apart
        v.push_back(Vertex(Vec3Df(-4.f + s, -4.f, 0.f), n));
        v.push_back(Vertex(Vec3Df(4.f + s, -4.f, 0.f), n));
        v.push_back(Vertex(Vec3Df(s, 4.f, 0.f), n));
        t.push_back(Triangle(3 * k, 3 * k + 1, 3 * k + 2));
    }
    const Mesh mesh(v, t);
    Bvh bvh;
    bvh.build(mesh, 1);
    CHECK_EQ(bvh.getNodes().size(), 31u);
    CHECK_EQ(bvh.getTriangleOrder()[0], 15u);  // leftmost leaf

    Vec3Df tilted(0.3f, 0.f, -1.f);
    tilted.normalize();
    const Ray rays[] = {Ray(Vec3Df(0.5f, -1.f, 3.f), Vec3Df(0.f, 0.f, -1.f)),
                        Ray(Vec3Df(0.5f, -1.f, 0.f) - 3.f * tilted, tilted)};
    for (const Ray& ray : rays) {
        Vertex a, b;
        float ta = std::numeric_limits<float>::max(), tb = 0.f;
        REQUIRE(bvh.nearestHit(ray, mesh, a, ta));
        REQUIRE(ray.nearestHit(mesh, b, tb));
        CHECK_EQ(ta, tb);
        CHECK(a.getNormal() == b.getNormal());
        CHECK_CLOSE(a.getNormal()[0], 0.f, 1e-6);  // copy 0
    }
}

TEST_CASE("bvh: only hits closer than the given distance are reported (nearest and any); an empty mesh hits nothing") {
    const Mesh quad = fixtures::quad(0.f, 1.f);
    Bvh bvh;
    bvh.build(quad);
    const Ray ray(Vec3Df(0.2f, 0.3f, 5.f), Vec3Df(0.f, 0.f, -1.f));
    Vertex hit;
    float t = 4.f;
    CHECK_MSG(!bvh.nearestHit(ray, quad, hit, t) && t == 4.f, "hit at 5 is beyond 4");
    t = 5.f;
    CHECK_MSG(!bvh.nearestHit(ray, quad, hit, t), "hit at exactly 5 is not closer than 5");
    t = 6.f;
    CHECK(bvh.nearestHit(ray, quad, hit, t));
    CHECK_CLOSE(t, 5.f, 1e-5);
    CHECK_MSG(!bvh.anyHit(ray, quad, t), "anyHit: nothing strictly before the hit");
    CHECK_MSG(bvh.anyHit(ray, quad, std::nextafter(t, 6.f)), "anyHit: the hit counts once the limit passes it");

    const Mesh empty;
    Bvh none;
    none.build(empty);
    CHECK(none.getNodes().empty());
    t = std::numeric_limits<float>::max();
    CHECK(!none.nearestHit(ray, empty, hit, t));
    CHECK(!none.anyHit(ray, empty, t));
}

TEST_CASE("bvh: 10 000 random rays hit exactly what brute force hits (teapot)") {
    const Scene s = loadModel("teapot", false);
    compareRays("teapot", s, randomRays(s.getBoundingBox(), 10000));
}
TEST_CASE("bvh: 10 000 random rays hit exactly what brute force hits (ram)") {
    const Scene s = loadModel("ram", false);
    compareRays("ram", s, randomRays(s.getBoundingBox(), 10000));
}
TEST_CASE("bvh: 10 000 random rays hit exactly what brute force hits (cube.obj, six objects)") {
    const Scene s = loadModel("cube.obj", false);
    compareRays("cube.obj", s, randomRays(s.getBoundingBox(), 10000));
}
TEST_CASE("bvh: 10 000 random rays hit exactly what brute force hits (teapot on its ground plane)") {
    const Scene s = loadModel("teapot", true);
    compareRays("teapot+ground", s, randomRays(s.getBoundingBox(), 10000));
}
TEST_CASE("bvh: 1 000 random rays hit exactly what brute force hits (minion, 84k triangles)") {
    const Scene s = loadModel("minion", false);
    compareRays("minion", s, randomRays(s.getBoundingBox(), 1000));
}

TEST_CASE("bvh: rays aimed exactly at triangle edges and corners hit what brute force hits") {
    compareRays("grid", fixtures::sceneOf(flatGrid(16, 0.25f)), edgeRays(fixtures::sceneOf(flatGrid(16, 0.25f))));
    const Scene cube = loadModel("cube.obj", true);
    compareRays("cube.obj+ground", cube, edgeRays(cube));
    const Scene teapot = loadModel("teapot", true);
    compareRays("teapot+ground", teapot, edgeRays(teapot));
}

TEST_CASE("bvh: renders and statistics are byte-identical with and without it") {
    struct Setup {
        const char* model;
        bool ground;
        RayTracer::DebugMode mode;
    };
    const Setup setups[] = {
        {"teapot", true, RayTracer::DebugMode::LIT},  // shadow rays onto a flat backdrop
        {"ram", true, RayTracer::DebugMode::LIT},     // the Rendu.png look
        {"cube.obj", false, RayTracer::DebugMode::OBJECT_ID},
    };
    for (const Setup& setup : setups) {
        const Scene s = loadModel(setup.model, setup.ground);
        const Camera cam = Camera::frame(s.getBoundingBox(), kPi / 4.f, 1.f, 2.f, 25.f, 20.f);
        RayTracer rt;
        rt.setDebugMode(setup.mode);
        rt.setAntiAliasing(2, true);  // jittered sub-pixel rays cross more silhouettes
        const Image withBvh = rt.render(s, cam, 48, 48);
        const RayTracer::Stats a = rt.getLastStats();
        rt.setBvhEnabled(false);
        const Image bruteForce = rt.render(s, cam, 48, 48);
        const RayTracer::Stats b = rt.getLastStats();

        REQUIRE(withBvh.sizeInBytes() == bruteForce.sizeInBytes());
        CHECK_MSG(std::memcmp(withBvh.data(), bruteForce.data(), withBvh.sizeInBytes()) == 0,
                  std::string(setup.model) + ": pixels differ");
        CHECK_EQ(a.rays, b.rays);
        CHECK_EQ(a.hits, b.hits);
        CHECK_EQ(a.minHitDist, b.minHitDist);
        CHECK_EQ(a.maxHitDist, b.maxHitDist);
    }
}
