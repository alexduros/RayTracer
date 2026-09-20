#include "Texture.h"

#include <cmath>
#include <iostream>

#include "Image.h"

namespace {

/// sRGB (IEC 61966-2-1) to linear: the inverse of what Display writes out.
inline float toLinear (unsigned char c) {
    const float s = static_cast<float> (c) / 255.f;
    return s <= 0.04045f ? s / 12.92f : std::pow ((s + 0.055f) / 1.055f, 2.4f);
}

/// Wrap an index into [0, n): a texture repeats.
inline int wrap (int i, int n) {
    const int m = i % n;
    return m < 0 ? m + n : m;
}

} // namespace

std::shared_ptr<const Texture> Texture::load (const std::string & filename) {
    Image image;
    if (!image.load (filename)) {
        std::cerr << "warning: cannot read the texture " << filename << " (keeping the material's colour)"
                  << std::endl;
        return nullptr;
    }
    const int w = image.width (), h = image.height ();
    std::vector<Vec3Df> texels (static_cast<size_t> (w) * h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            unsigned char r = 0, g = 0, b = 0;
            image.getPixel (x, y, r, g, b);
            texels[static_cast<size_t> (y) * w + x] = Vec3Df (toLinear (r), toLinear (g), toLinear (b));
        }
    return std::shared_ptr<const Texture> (new Texture (w, h, std::move (texels), filename));
}

const Vec3Df & Texture::texel (int x, int y) const {
    return texels[static_cast<size_t> (wrap (y, h)) * w + wrap (x, w)];
}

Vec3Df Texture::sample (float u, float v) const {
    if (texels.empty ())
        return Vec3Df (1.f, 1.f, 1.f);
    // Texel centres sit at half-integers, and OBJ's v grows upward while the
    // image's rows grow downward.
    const float x = u * static_cast<float> (w) - 0.5f;
    const float y = (1.f - v) * static_cast<float> (h) - 0.5f;
    const float x0 = std::floor (x), y0 = std::floor (y);
    const float fx = x - x0, fy = y - y0;
    const int ix = static_cast<int> (x0), iy = static_cast<int> (y0);
    return (1.f - fx) * (1.f - fy) * texel (ix, iy) + fx * (1.f - fy) * texel (ix + 1, iy) +
           (1.f - fx) * fy * texel (ix, iy + 1) + fx * fy * texel (ix + 1, iy + 1);
}
