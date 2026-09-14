// Reproducible random numbers for the sampled effects: anti-aliasing jitter,
// soft shadows, and later ambient occlusion and depth of field.
//
// A sampler is seeded from the pixel it serves, so a picture does not depend
// on tile order or thread count, and each effect draws from its own stream,
// so turning one effect on does not reshuffle another's samples.
#ifndef SAMPLER_H
#define SAMPLER_H

#include <random>

class Sampler {
public:
    enum class Stream : unsigned int { PIXEL_JITTER = 0, SHADOWS = 1 };

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

private:
    std::minstd_rand rng;
};

#endif // SAMPLER_H
