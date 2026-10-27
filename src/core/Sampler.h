// Reproducible random numbers for the sampled effects: anti-aliasing jitter,
// soft shadows, ambient occlusion, depth of field.
//
// A sampler is seeded from the pixel it serves, so a picture does not depend
// on tile order or thread count, and each effect draws from its own stream,
// so turning one effect on does not reshuffle another's samples.
#ifndef SAMPLER_H
#define SAMPLER_H

#include <cmath>
#include <random>

class Sampler {
public:
    enum class Stream : unsigned int { PIXEL_JITTER = 0, SHADOWS = 1, AMBIENT_OCCLUSION = 2, LENS = 3 };

    explicit Sampler (unsigned int seed = 1u) : rng (seed) {}

    /// Seeded from the pixel and the stream only.
    static inline Sampler forPixel (unsigned int x, unsigned int y, Stream stream) {
        return Sampler ((x * 73856093u) ^ (y * 19349663u) ^
                        (0x9E3779B9u + static_cast<unsigned int> (stream) * 0x85EBCA6Bu));
    }

    /// Uniform in [0, 1] from the raw engine output. std::minstd_rand is fully
    /// specified by the standard, unlike the distributions, so every platform
    /// draws the same numbers.
    inline float next () {
        return static_cast<float> (rng () - std::minstd_rand::min ()) /
               (static_cast<float> (std::minstd_rand::max () - std::minstd_rand::min ()) + 1.f);
    }

    /// Maps [0,1]^2 onto the unit disk so that equal areas stay equal and
    /// neighbouring points stay neighbours: jittered grid cells land on the
    /// disk as compact patches (P. Shirley & K. Chiu's concentric map).
    static inline void concentricDisk (float a, float b, float & x, float & y) {
        const float sx = 2.f * a - 1.f, sy = 2.f * b - 1.f;
        if (sx == 0.f && sy == 0.f) {
            x = y = 0.f;
            return;
        }
        const float quarterPi = 0.78539816f;
        float r, theta;
        if (std::fabs (sx) > std::fabs (sy)) {
            r = sx;
            theta = quarterPi * (sy / sx);
        } else {
            r = sy;
            theta = 2.f * quarterPi - quarterPi * (sx / sy);
        }
        x = r * std::cos (theta);
        y = r * std::sin (theta);
    }

private:
    std::minstd_rand rng;
};

/// The samplers a ray's shading draws from, one per sampled effect, so that
/// turning one effect on never shifts another's numbers. Reflected rays share
/// their pixel's.
struct PixelSamplers {
    Sampler shadows;
    Sampler occlusion;

    /// Fixed seeds: what RayTracer::trace uses to probe a single ray.
    PixelSamplers () : shadows (1u), occlusion (2u) {}
    PixelSamplers (const Sampler & shadows, const Sampler & occlusion) : shadows (shadows), occlusion (occlusion) {}

    /// Seeded from the pixel, each effect from its own stream.
    static inline PixelSamplers forPixel (unsigned int x, unsigned int y) {
        return PixelSamplers (Sampler::forPixel (x, y, Sampler::Stream::SHADOWS),
                              Sampler::forPixel (x, y, Sampler::Stream::AMBIENT_OCCLUSION));
    }
};

#endif // SAMPLER_H
