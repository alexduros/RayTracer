// Wrinkles without geometry: the surface stays where it is and only the
// normal the light is computed with follows a height map.
//
// J. F. Blinn, "Simulation of Wrinkled Surfaces", SIGGRAPH 1978. A surface
// P(u, v) pushed out along its normal by a small height F(u, v),
//
//     P' = P + F N / |N|,        N = Pu x Pv,
//
// has, to first order in F, the normal
//
//     N' = N + (Fu (N x Pv) - Fv (N x Pu)) / |N|,
//
// where Pu, Pv, Fu, Fv are the derivatives along the surface's own
// coordinates. Nothing else of P' is ever computed: silhouettes and the
// outlines of shadows stay smooth, which is the limit the paper states.
//
// Tested on their own in tests/TestBump.cpp.
#ifndef BUMP_H
#define BUMP_H

#include <cmath>

#include "Vec3D.h"
#include "Vertex.h"

namespace bump {

/// Pu and Pv over a triangle: how far the surface moves, in world units, per
/// unit of u and of v. Constant over the triangle, since position and
/// coordinates are both linear on it. False when the corners carry no usable
/// coordinates (an OFF model reads (0, 0) everywhere): nothing to derive.
inline bool tangents (const Vertex & a, const Vertex & b, const Vertex & c, Vec3Df & pu, Vec3Df & pv) {
    const Vec3Df e1 = b.getPos () - a.getPos (), e2 = c.getPos () - a.getPos ();
    const float du1 = b.getU () - a.getU (), dv1 = b.getV () - a.getV ();
    const float du2 = c.getU () - a.getU (), dv2 = c.getV () - a.getV ();
    // The area of the triangle in the (u, v) plane, twice. Nothing, or
    // nothing but the rounding of its two products: no usable coordinates.
    const float det = du1 * dv2 - du2 * dv1;
    if (!(std::fabs (det) > 1e-5f * (std::fabs (du1 * dv2) + std::fabs (du2 * dv1))))
        return false;
    pu = (dv2 * e1 - dv1 * e2) / det;
    pv = (du1 * e2 - du2 * e1) / det;
    return true;
}

/// Blinn's perturbed normal, unit length: the unit normal `n` of a surface
/// whose tangents are `pu` and `pv`, tilted by the slopes `fu` = dF/du and
/// `fv` = dF/dv of a height F measured in world units along `n`. A slope of
/// s world units per world unit tilts it by atan(s), away from the rise.
///
/// `n` is the normal being shaded, which on a smooth mesh is interpolated
/// and not exactly Pu x Pv / |Pu x Pv|: |N| is taken as n . (Pu x Pv), and
/// its sign tells coordinates that run mirrored over the surface, where the
/// tilt must change sides for "white is high" to keep meaning outward.
/// Where the map is flat, `n` comes back untouched, bit for bit.
inline Vec3Df perturb (const Vec3Df & n, const Vec3Df & pu, const Vec3Df & pv, float fu, float fv) {
    if (fu == 0.f && fv == 0.f)
        return n;
    const float area = Vec3Df::dotProduct (n, Vec3Df::crossProduct (pu, pv));
    const Vec3Df d = fu * Vec3Df::crossProduct (n, pv) - fv * Vec3Df::crossProduct (n, pu);
    const Vec3Df tilted = std::fabs (area) * n + (area < 0.f ? -1.f : 1.f) * d;
    const float length = tilted.getLength ();
    if (!(length > 0.f) || !std::isfinite (length))
        return n;  // tangents too degenerate to tilt anything
    return tilted / length;
}

} // namespace bump

#endif // BUMP_H
