// Geometric optics of a smooth surface: the mirror direction, Snell's law
// and the Fresnel equations. Used by the mirror and glass materials in
// RayTracer::shade; tested on their own in tests/TestRefraction.cpp.
//
// Directions are unit vectors, `n` the unit normal on the side the ray comes
// from (d.n < 0), indices of refraction relative to vacuum.
//
// References: T. Whitted, "An Improved Illumination Model for Shaded
// Display", CACM 23(6), 1980; M. Born & E. Wolf, "Principles of Optics",
// section 1.5 (the Fresnel equations).
#ifndef OPTICS_H
#define OPTICS_H

#include <algorithm>
#include <cmath>

#include "Vec3D.h"

namespace optics {

/// `d` mirrored about the surface: angle out = angle in.
inline Vec3Df reflect (const Vec3Df & d, const Vec3Df & n) {
    return d - (2.f * Vec3Df::dotProduct (d, n)) * n;
}

/// `d` bent through the surface from index n1 into n2, with eta = n1 / n2:
/// n1 sin(i) = n2 sin(t). False when sin(t) would exceed 1, past the critical
/// angle: total internal reflection, nothing gets through.
inline bool refract (const Vec3Df & d, const Vec3Df & n, float eta, Vec3Df & t) {
    const float cosi = -Vec3Df::dotProduct (d, n);
    const float sin2t = eta * eta * std::max (0.f, 1.f - cosi * cosi);
    if (sin2t > 1.f)
        return false;
    const float cost = std::sqrt (1.f - sin2t);
    t = eta * d + (eta * cosi - cost) * n;
    return true;
}

/// Share of the light a smooth dielectric reflects, going from index n1 into
/// n2 at an angle of incidence whose cosine is `cosi`: the Fresnel equations
/// for unpolarised light, the mean of the s and p reflectances. The rest is
/// transmitted. 0 between equal indices, ((n1 - n2) / (n1 + n2))^2 at normal
/// incidence, 1 at grazing and past the critical angle.
inline float fresnel (float cosi, float n1, float n2) {
    if (n1 == n2)
        return 0.f;  // no interface, even at grazing
    cosi = std::clamp (cosi, 0.f, 1.f);
    const float sint = n1 / n2 * std::sqrt (std::max (0.f, 1.f - cosi * cosi));
    if (sint >= 1.f)
        return 1.f;
    const float cost = std::sqrt (std::max (0.f, 1.f - sint * sint));
    const float rs = (n1 * cosi - n2 * cost) / (n1 * cosi + n2 * cost);
    const float rp = (n2 * cosi - n1 * cost) / (n2 * cosi + n1 * cost);
    return 0.5f * (rs * rs + rp * rp);
}

} // namespace optics

#endif // OPTICS_H
