// A highlight made of tiny mirrors: the surface is a field of facets whose
// slopes follow a distribution, and only those turned to mirror the light
// into the eye shine.
//
// R. L. Cook & K. E. Torrance, "A Reflectance Model for Computer Graphics",
// SIGGRAPH 1981 (revised in ACM Transactions on Graphics 1(1), 1982), after
// K. E. Torrance & E. M. Sparrow, "Theory for Off-Specular Reflection from
// Roughened Surfaces", JOSA 57(9), 1967. The specular reflectance is
//
//     f(l, v) = D(h) G(l, v, h) F(v . h) / (4 (n . l) (n . v)),
//
// h the half vector between the light l and the eye v: D the share of facets
// facing h, G the share of those neither hidden from the light nor from the
// eye by their neighbours, F the Fresnel reflectance of one facet. The paper
// divides by pi instead of 4; 4 is the factor its own derivation implies
// once D is a density over directions (B. Walter et al., "Microfacet Models
// for Refraction through Rough Surfaces", EGSR 2007), and with it a mirror
// is the limit of a smooth surface.
//
// Tested on their own in tests/TestMicrofacet.cpp.
#ifndef MICROFACET_H
#define MICROFACET_H

#include <algorithm>
#include <cmath>

#include "Optics.h"
#include "Vec3D.h"

namespace microfacet {

constexpr float kPi = 3.14159265358979f;

/// Beckmann's distribution of facet normals around the surface normal, m
/// the root mean square slope of the facets (0.05 polished, 0.5 rough), as a
/// density over directions: the facets project onto the surface exactly
/// once, the integral of D(h) (n . h) over the hemisphere is 1.
/// D = exp(-tan^2(a) / m^2) / (pi m^2 cos^4(a)), a the angle of h to n.
inline float beckmann (float nDotH, float m) {
    if (nDotH <= 0.f || m <= 0.f)
        return 0.f;
    const float c2 = nDotH * nDotH, m2 = m * m;
    const float tan2 = (1.f - c2) / c2;
    return std::exp (-tan2 / m2) / (kPi * m2 * c2 * c2);
}

/// Cook and Torrance's geometric attenuation, from Torrance and Sparrow's
/// V-shaped grooves: the share of a facet facing h that is lit and seen,
/// min(1, 2 (n.h)(n.v) / (v.h), 2 (n.h)(n.l) / (v.h)). Symmetric in l and v.
inline float geometry (float nDotH, float nDotV, float nDotL, float vDotH) {
    if (vDotH <= 0.f)
        return 0.f;
    const float seen = 2.f * nDotH * nDotV / vDotH, lit = 2.f * nDotH * nDotL / vDotH;
    return std::min (1.f, std::min (seen, lit));
}

/// The index a dielectric would need to reflect `f0` head-on, the inverse of
/// f0 = ((n - 1) / (n + 1))^2: how Cook and Torrance put a metal, known by
/// its reflectance at normal incidence, into the Fresnel equations, one
/// channel at a time. 0 gives 1 (no interface); held below 1, where n would
/// be infinite.
inline float iorFromReflectance (float f0) {
    const float s = std::sqrt (std::clamp (f0, 0.f, 0.9999f));
    return (1.f + s) / (1.f - s);
}

/// The specular reflectance f(l, v) above, for unit vectors n (the normal),
/// v (toward the eye) and l (toward the light), roughness m, and a facet of
/// index `ior` in air. 0 when either direction is below the surface.
inline float reflectance (const Vec3Df & n, const Vec3Df & v, const Vec3Df & l, float m, float ior) {
    const float nDotL = Vec3Df::dotProduct (n, l), nDotV = Vec3Df::dotProduct (n, v);
    if (nDotL <= 0.f || nDotV <= 0.f)
        return 0.f;
    Vec3Df h = l + v;
    if (h.normalize () <= 0.f)
        return 0.f;
    const float nDotH = Vec3Df::dotProduct (n, h), vDotH = Vec3Df::dotProduct (v, h);
    const float d = beckmann (nDotH, m), g = geometry (nDotH, nDotV, nDotL, vDotH);
    const float f = optics::fresnel (vDotH, 1.f, ior);
    return d * g * f / (4.f * nDotL * nDotV);
}

} // namespace microfacet

#endif // MICROFACET_H
