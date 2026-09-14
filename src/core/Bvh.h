// Bounding-volume hierarchy over the triangles of one mesh.
//
// A binary tree of axis-aligned boxes stored in a flat array: a ray that
// misses a box skips every triangle below it, so a hit costs a few dozen
// triangle tests instead of one per triangle. The tree holds indices only,
// never pointers, so it is copied safely with the Object that owns it.
//
// The BVH changes the speed, never the picture: nearestHit returns exactly
// the triangle Ray::nearestHit finds by scanning every triangle, and a test
// compares the two on random rays and on whole renders.
//
// Reference: T. L. Kay & J. T. Kajiya, "Ray Tracing Complex Scenes",
// SIGGRAPH 1986.
#ifndef BVH_H
#define BVH_H

#include <vector>

#include "BoundingBox.h"
#include "Mesh.h"
#include "Ray.h"
#include "Vertex.h"

class Bvh {
public:
    /// One node of the flat array. An inner node's left child is the next
    /// node in the array and `index` is its right child; a leaf covers the
    /// triangles getTriangleOrder()[index .. index + count - 1].
    struct Node {
        BoundingBox box;
        unsigned int index = 0;
        unsigned int count = 0;  // 0 for an inner node
        inline bool isLeaf () const { return count > 0; }
    };

    static constexpr unsigned int kDefaultLeafSize = 4;

    Bvh () {}

    /// Split the triangles at the median of their centroids along the
    /// longest axis, recursively, until at most `maxLeafSize` remain. Boxes
    /// are grown by a margin well above rounding error, so a ray the triangle
    /// test accepts always enters every box on the way to that triangle.
    void build (const Mesh & mesh, unsigned int maxLeafSize = kDefaultLeafSize);

    /// Closest front-facing triangle of `mesh`, which must be the mesh the
    /// tree was built from. `t` is read as the farthest distance worth
    /// reporting: only hits strictly closer are returned (a closer object
    /// already won the rest), and on success `t` and `hit` are updated. At
    /// equal distance the lowest triangle index wins, which is what the scan
    /// in Ray::nearestHit gives, so both return the same triangle.
    bool nearestHit (const Ray & ray, const Mesh & mesh, Vertex & hit, float & t) const;

    /// True when some front-facing triangle of `mesh` is hit strictly closer
    /// than `tMax`. Stops at the first one found: the question a shadow ray
    /// asks, cheaper than looking for the closest.
    bool anyHit (const Ray & ray, const Mesh & mesh, float tMax) const;

    inline const std::vector<Node> & getNodes () const { return nodes; }
    inline const std::vector<unsigned int> & getTriangleOrder () const { return order; }

private:
    std::vector<Node> nodes;          // nodes[0] is the root; empty for a mesh without triangles
    std::vector<unsigned int> order;  // triangle indices, each leaf a contiguous range
};

#endif // BVH_H
