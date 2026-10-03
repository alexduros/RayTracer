#include "Environment.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace {

const float kPi = 3.14159265358979f;

} // namespace

std::shared_ptr<const Environment> Environment::load (const std::string & filename) {
    const std::shared_ptr<const Texture> map = Texture::read (filename);
    if (!map) {
        std::cerr << "warning: cannot read the environment map " << filename << std::endl;
        return nullptr;
    }
    return std::make_shared<const Environment> (map);
}

void Environment::toMap (const Vec3Df & direction, float & u, float & v) {
    Vec3Df d = direction;
    d.normalize ();
    u = 0.5f + std::atan2 (d[0], -d[2]) / (2.f * kPi);
    v = std::acos (std::clamp (d[1], -1.f, 1.f)) / kPi;
}

Vec3Df Environment::toDirection (float u, float v) {
    const float azimuth = (u - 0.5f) * 2.f * kPi, polar = v * kPi;
    return Vec3Df (std::sin (polar) * std::sin (azimuth), std::cos (polar), -std::sin (polar) * std::cos (azimuth));
}

Vec3Df Environment::sample (const Vec3Df & direction) const {
    float u, v;
    toMap (direction, u, v);
    // A texture wraps both ways, which is right around the horizon and wrong
    // over the poles: the zenith would blend with the nadir. Held half a
    // texel inside, the read never leaves the top or the bottom row.
    const float edge = 0.5f / static_cast<float> (map->height ());
    v = std::clamp (v, edge, 1.f - edge);
    // The texture counts v upward, as OBJ files do.
    return map->sample (u, 1.f - v);
}
