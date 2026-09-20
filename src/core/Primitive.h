// Surfaces given by an equation instead of triangles: a ray meets them where
// a polynomial vanishes, so the hit is exact at any zoom and costs one root
// instead of thousands of triangle tests.
//
// This is how the first useful ray caster worked: R. A. Goldstein & R. Nagel,
// "3-D Visual Simulation", Simulation 16(1), 1971 (MAGI's SynthaVision),
// where solids were quadrics combined by boolean operators. The intervals
// their combination needs come later (S. D. Roth, "Ray Casting for Modeling
// Solids", CGIP 18(2), 1982).
#ifndef PRIMITIVE_H
#define PRIMITIVE_H

#include <cmath>

#include "BoundingBox.h"
#include "Ray.h"
#include "Vec3D.h"
#include "Vertex.h"

class Primitive {
public:
    virtual ~Primitive () {}

    /// Closest hit strictly closer than `t`, which is read as the farthest
    /// distance worth reporting and updated on success, exactly as
    /// Bvh::nearestHit does. `hit` receives the point and the outward normal.
    /// Back faces are skipped unless the ray is two-sided, like triangles.
    virtual bool intersect (const Ray & ray, Vertex & hit, float & t) const = 0;

    /// Axis-aligned box holding the whole surface.
    virtual BoundingBox boundingBox () const = 0;

protected:
    /// Shared tail of every intersect: keep the nearest root that lies ahead,
    /// is closer than `t`, and faces the ray (unless it is two-sided).
    static bool keep (const Ray & ray, const Vec3Df & position, const Vec3Df & normal, float candidate, Vertex & hit,
                      float & t) {
        if (candidate <= 0.f || candidate >= t)
            return false;
        if (!ray.isTwoSided () && Vec3Df::dotProduct (normal, ray.getDirection ()) > 0.f)
            return false;
        hit.setPos (position);
        hit.setNormal (normal);
        t = candidate;
        return true;
    }
};

/// |p - centre| = radius. The quadratic of Goldstein & Nagel's spheres.
class Sphere : public Primitive {
public:
    Sphere (const Vec3Df & centre, float radius) : centre (centre), radius (radius) {}

    const Vec3Df & getCentre () const { return centre; }
    float getRadius () const { return radius; }

    bool intersect (const Ray & ray, Vertex & hit, float & t) const override {
        // |o + s d - c|^2 = r^2 with d a unit vector: s^2 + 2 b s + c0 = 0.
        const Vec3Df oc = ray.getOrigin () - centre;
        const float b = Vec3Df::dotProduct (oc, ray.getDirection ());
        const float c0 = Vec3Df::dotProduct (oc, oc) - radius * radius;
        const float discriminant = b * b - c0;
        if (discriminant < 0.f)
            return false;
        const float root = std::sqrt (discriminant);
        // Near root first: the far one is the exit, kept only by a two-sided ray.
        for (const float s : {-b - root, -b + root}) {
            const Vec3Df p = ray.getOrigin () + s * ray.getDirection ();
            Vec3Df n = (p - centre) / radius;
            n.normalize ();
            if (keep (ray, p, n, s, hit, t))
                return true;
        }
        return false;
    }

    BoundingBox boundingBox () const override {
        const Vec3Df r (radius, radius, radius);
        return BoundingBox (centre - r, centre + r);
    }

private:
    Vec3Df centre;
    float radius;
};

/// The side of a cylinder: the points at `radius` from the segment that
/// leaves `base` along `axis` for `height`. Open at both ends; cap it with
/// discs.
class Cylinder : public Primitive {
public:
    Cylinder (const Vec3Df & base, const Vec3Df & axis, float radius, float height)
        : base (base), axis (axis), radius (radius), height (height) {
        this->axis.normalize ();
    }

    bool intersect (const Ray & ray, Vertex & hit, float & t) const override {
        // The same quadratic as a sphere, in the plane perpendicular to the axis.
        const Vec3Df d = ray.getDirection (), oc = ray.getOrigin () - base;
        const Vec3Df dp = d - Vec3Df::dotProduct (d, axis) * axis;
        const Vec3Df op = oc - Vec3Df::dotProduct (oc, axis) * axis;
        const float a = Vec3Df::dotProduct (dp, dp);
        if (a < 1e-12f)
            return false;  // parallel to the axis: it can only meet the caps
        const float b = Vec3Df::dotProduct (dp, op), c0 = Vec3Df::dotProduct (op, op) - radius * radius;
        const float discriminant = b * b - a * c0;
        if (discriminant < 0.f)
            return false;
        const float root = std::sqrt (discriminant);
        for (const float s : {(-b - root) / a, (-b + root) / a}) {
            const Vec3Df p = ray.getOrigin () + s * d;
            const float along = Vec3Df::dotProduct (p - base, axis);
            if (along < 0.f || along > height)
                continue;  // beyond an end
            Vec3Df n = p - base - along * axis;
            n.normalize ();
            if (keep (ray, p, n, s, hit, t))
                return true;
        }
        return false;
    }

    BoundingBox boundingBox () const override {
        // Along each axis the round section reaches radius x sqrt(1 - a_i^2).
        Vec3Df extent;
        for (int i = 0; i < 3; ++i)
            extent[i] = radius * std::sqrt (std::max (0.f, 1.f - axis[i] * axis[i]));
        const Vec3Df top = base + height * axis;
        Vec3Df lo, hi;
        for (int i = 0; i < 3; ++i) {
            lo[i] = std::min (base[i], top[i]) - extent[i];
            hi[i] = std::max (base[i], top[i]) + extent[i];
        }
        return BoundingBox (lo, hi);
    }

private:
    Vec3Df base, axis;
    float radius, height;
};

/// A flat disc: the points of the plane through `centre` with unit normal
/// `normal` that lie within `radius`. A cylinder's cap, or a round floor.
class Disc : public Primitive {
public:
    Disc (const Vec3Df & centre, const Vec3Df & normal, float radius)
        : centre (centre), normal (normal), radius (radius) {
        this->normal.normalize ();
    }

    bool intersect (const Ray & ray, Vertex & hit, float & t) const override {
        const float denominator = Vec3Df::dotProduct (ray.getDirection (), normal);
        if (std::fabs (denominator) < 1e-9f)
            return false;  // parallel to the plane
        const float s = Vec3Df::dotProduct (centre - ray.getOrigin (), normal) / denominator;
        const Vec3Df p = ray.getOrigin () + s * ray.getDirection ();
        if ((p - centre).getSquaredLength () > radius * radius)
            return false;
        return keep (ray, p, normal, s, hit, t);
    }

    BoundingBox boundingBox () const override {
        Vec3Df extent;
        for (int i = 0; i < 3; ++i)
            extent[i] = radius * std::sqrt (std::max (0.f, 1.f - normal[i] * normal[i]));
        return BoundingBox (centre - extent, centre + extent);
    }

private:
    Vec3Df centre, normal;
    float radius;
};

#endif // PRIMITIVE_H
