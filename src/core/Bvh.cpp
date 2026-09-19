// Bounding-volume hierarchy: see Bvh.h.
#include "Bvh.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

/// Boxes grow by this fraction of the mesh's size or coordinate magnitude,
/// whichever is larger: far above float rounding in the slab and triangle
/// tests, far below anything that costs traversal time. Without it a ray
/// grazing a silhouette, or crossing a flat box (the ground plane), could be
/// accepted by the triangle test yet miss the leaf's box.
constexpr float kMarginFactor = 1e-4f;

/// Median splits halve the triangle count at every level, so the tree is at
/// most 33 levels deep for 2^32 triangles; traversal pushes at most one entry
/// per level.
constexpr unsigned int kStackSize = 64;

struct Builder {
    const std::vector<Triangle> & triangles;
    const std::vector<Vertex> & vertices;
    const std::vector<Vec3Df> & centroids;
    std::vector<Bvh::Node> & nodes;
    std::vector<unsigned int> & order;
    unsigned int maxLeafSize;
    float margin;

    /// Node for order[begin, end); returns its index.
    unsigned int build (unsigned int begin, unsigned int end) {
        const unsigned int index = static_cast<unsigned int> (nodes.size ());
        nodes.push_back (Bvh::Node ());

        BoundingBox box (vertices[triangles[order[begin]].getVertex (0)].getPos ());
        BoundingBox centroidBox (centroids[order[begin]]);
        for (unsigned int k = begin; k < end; ++k) {
            const Triangle & triangle = triangles[order[k]];
            for (unsigned int v = 0; v < 3; ++v)
                box.extendTo (vertices[triangle.getVertex (v)].getPos ());
            centroidBox.extendTo (centroids[order[k]]);
        }
        const Vec3Df pad (margin, margin, margin);
        nodes[index].box = BoundingBox (box.getMin () - pad, box.getMax () + pad);

        const unsigned int count = end - begin;
        if (count <= maxLeafSize) {
            nodes[index].index = begin;
            nodes[index].count = count;
            return index;
        }
        // Equal coordinates are ordered by triangle index, so the partition
        // is the same with every standard library's nth_element.
        const int axis = centroidBox.getDirection ();
        const unsigned int mid = begin + count / 2;
        std::nth_element (order.begin () + begin, order.begin () + mid, order.begin () + end,
                          [&] (unsigned int a, unsigned int b) {
                              const float ca = centroids[a][axis], cb = centroids[b][axis];
                              return ca < cb || (ca == cb && a < b);
                          });
        build (begin, mid);  // the left child is always the next node
        const unsigned int right = build (mid, end);
        nodes[index].index = right;
        return index;
    }
};

} // namespace

void Bvh::build (const Mesh & mesh, unsigned int maxLeafSize) {
    nodes.clear ();
    order.clear ();
    const std::vector<Triangle> & triangles = mesh.getTriangles ();
    const std::vector<Vertex> & vertices = mesh.getVertices ();
    if (triangles.empty ())
        return;

    std::vector<Vec3Df> centroids (triangles.size ());
    order.resize (triangles.size ());
    BoundingBox all (vertices[triangles[0].getVertex (0)].getPos ());
    for (unsigned int i = 0; i < triangles.size (); ++i) {
        Vec3Df sum;
        for (unsigned int v = 0; v < 3; ++v) {
            const Vec3Df & p = vertices[triangles[i].getVertex (v)].getPos ();
            sum += p;
            all.extendTo (p);
        }
        centroids[i] = sum / 3.f;
        order[i] = i;
    }
    float magnitude = all.getSize ();
    for (int i = 0; i < 3; ++i)
        magnitude = std::max (magnitude, std::max (std::fabs (all.getMin ()[i]), std::fabs (all.getMax ()[i])));

    Builder builder {triangles, vertices, centroids, nodes, order, std::max (1u, maxLeafSize),
                     kMarginFactor * magnitude};
    builder.build (0, static_cast<unsigned int> (triangles.size ()));
}

bool Bvh::nearestHit (const Ray & ray, const Mesh & mesh, Vertex & hit, float & t, unsigned int & triangle) const {
    if (nodes.empty ())
        return false;
    const Vec3Df & d = ray.getDirection ();
    const Vec3Df invDirection (1.f / d[0], 1.f / d[1], 1.f / d[2]);  // +-inf on an axis-parallel ray
    const std::vector<Triangle> & triangles = mesh.getTriangles ();

    bool found = false;
    float best = t;
    unsigned int bestTriangle = 0;
    Vertex candidate;
    float tc = 0.f;

    struct Entry {
        unsigned int node;
        float tEntry;
    };
    Entry stack[kStackSize];
    unsigned int size = 0;
    float tEntry = 0.f;
    if (!ray.intersect (nodes[0].box, invDirection, best, tEntry))
        return false;
    stack[size++] = {0, tEntry};

    while (size > 0) {
        const Entry entry = stack[--size];
        // Strictly beyond: a box entered exactly at `best` may hold a tie
        // with a lower triangle index.
        if (entry.tEntry > best)
            continue;
        const Node & node = nodes[entry.node];
        if (node.isLeaf ()) {
            for (unsigned int k = node.index; k < node.index + node.count; ++k) {
                const unsigned int i = order[k];
                if (ray.hit (triangles[i], mesh, candidate, tc) &&
                    (tc < best || (found && tc == best && i < bestTriangle))) {
                    best = tc;
                    bestTriangle = i;
                    hit = candidate;
                    found = true;
                }
            }
            continue;
        }
        // Visit the nearer child first; the farther one waits on the stack
        // with its entry distance, and is skipped if a closer hit turns up.
        unsigned int nearChild = entry.node + 1, farChild = node.index;
        float tNear = 0.f, tFar = 0.f;
        const bool hitNear = ray.intersect (nodes[nearChild].box, invDirection, best, tNear);
        const bool hitFar = ray.intersect (nodes[farChild].box, invDirection, best, tFar);
        if (hitNear && hitFar) {
            if (tFar < tNear) {
                std::swap (nearChild, farChild);
                std::swap (tNear, tFar);
            }
            stack[size++] = {farChild, tFar};
            stack[size++] = {nearChild, tNear};
        } else if (hitNear) {
            stack[size++] = {nearChild, tNear};
        } else if (hitFar) {
            stack[size++] = {farChild, tFar};
        }
    }
    if (found) {
        t = best;
        triangle = bestTriangle;
    }
    return found;
}

bool Bvh::anyHit (const Ray & ray, const Mesh & mesh, float tMax) const {
    if (nodes.empty ())
        return false;
    const Vec3Df & d = ray.getDirection ();
    const Vec3Df invDirection (1.f / d[0], 1.f / d[1], 1.f / d[2]);
    const std::vector<Triangle> & triangles = mesh.getTriangles ();
    Vertex candidate;
    float tc = 0.f, tEntry = 0.f;

    unsigned int stack[kStackSize];
    unsigned int size = 0;
    stack[size++] = 0;
    while (size > 0) {
        const unsigned int index = stack[--size];
        const Node & node = nodes[index];
        if (!ray.intersect (node.box, invDirection, tMax, tEntry))
            continue;
        if (node.isLeaf ()) {
            for (unsigned int k = node.index; k < node.index + node.count; ++k)
                if (ray.hit (triangles[order[k]], mesh, candidate, tc) && tc < tMax)
                    return true;
            continue;
        }
        stack[size++] = node.index;  // right child
        stack[size++] = index + 1;   // left child
    }
    return false;
}
