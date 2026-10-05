#include "Texture.h"

#include <cmath>
#include <iostream>

#include "HdrImage.h"
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
    const std::shared_ptr<const Texture> texture = read (filename);
    if (!texture)
        std::cerr << "warning: cannot read the texture " << filename << " (keeping the material's colour)"
                  << std::endl;
    return texture;
}

std::shared_ptr<const Texture> Texture::read (const std::string & filename) {
    return read (filename, true);
}

std::shared_ptr<const Texture> Texture::readData (const std::string & filename) {
    return read (filename, false);
}

std::shared_ptr<const Texture> Texture::fromTexels (int width, int height, std::vector<Vec3Df> texels) {
    if (width <= 0 || height <= 0 || texels.size () != static_cast<size_t> (width) * height)
        return nullptr;
    return std::shared_ptr<const Texture> (new Texture (width, height, std::move (texels), std::string ()));
}

std::shared_ptr<const Texture> Texture::read (const std::string & filename, bool decodeSrgb) {
    if (HdrImage::isRadiance (filename)) {
        // Floats, linear, and not limited to 1: nothing to decode.
        HdrImage hdr;
        if (!hdr.load (filename))
            return nullptr;
        const int w = hdr.width (), h = hdr.height ();
        std::vector<Vec3Df> texels (static_cast<size_t> (w) * h);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                texels[static_cast<size_t> (y) * w + x] = hdr.get (x, y);
        return std::shared_ptr<const Texture> (new Texture (w, h, std::move (texels), filename));
    }
    Image image;
    if (!image.load (filename))
        return nullptr;
    const int w = image.width (), h = image.height ();
    std::vector<Vec3Df> texels (static_cast<size_t> (w) * h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            unsigned char r = 0, g = 0, b = 0;
            image.getPixel (x, y, r, g, b);
            texels[static_cast<size_t> (y) * w + x] =
                decodeSrgb ? Vec3Df (toLinear (r), toLinear (g), toLinear (b))
                           : Vec3Df (static_cast<float> (r), static_cast<float> (g), static_cast<float> (b)) / 255.f;
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

float Texture::grey (int x, int y) const {
    const Vec3Df & t = texel (x, y);
    return (t[0] + t[1] + t[2]) / 3.f;
}

void Texture::heightGradient (float u, float v, float & dhdu, float & dhdv) const {
    dhdu = dhdv = 0.f;
    if (texels.empty ())
        return;
    // The same four texels and weights as `sample`.
    const float x = u * static_cast<float> (w) - 0.5f;
    const float y = (1.f - v) * static_cast<float> (h) - 0.5f;
    const float x0 = std::floor (x), y0 = std::floor (y);
    const float fx = x - x0, fy = y - y0;
    const int ix = static_cast<int> (x0), iy = static_cast<int> (y0);
    for (int j = 0; j < 2; ++j)
        for (int i = 0; i < 2; ++i) {
            const float weight = (i ? fx : 1.f - fx) * (j ? fy : 1.f - fy);
            const int tx = ix + i, ty = iy + j;
            dhdu += weight * (grey (tx + 1, ty) - grey (tx - 1, ty));
            dhdv += weight * (grey (tx, ty - 1) - grey (tx, ty + 1));  // rows grow downward, v upward
        }
    // A difference across two texels, each 1 / w of u (1 / h of v) wide.
    dhdu *= 0.5f * static_cast<float> (w);
    dhdv *= 0.5f * static_cast<float> (h);
}
