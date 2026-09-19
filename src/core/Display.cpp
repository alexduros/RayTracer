#include "Display.h"

#include <algorithm>
#include <cmath>

namespace {

inline float luminance (const Vec3Df & c) {
    return 0.2126f * c[0] + 0.7152f * c[1] + 0.0722f * c[2];  // Rec. 709 / sRGB primaries
}

inline float aces (float x) {
    x = std::max (0.f, x);
    return std::clamp ((x * (2.51f * x + 0.03f)) / (x * (2.43f * x + 0.59f) + 0.14f), 0.f, 1.f);
}

inline float srgb (float c) {
    return c <= 0.0031308f ? 12.92f * c : 1.055f * std::pow (c, 1.f / 2.4f) - 0.055f;
}

inline unsigned char toByte (float c) {
    const int v = static_cast<int> (c * 255.f + 0.5f);
    return static_cast<unsigned char> (std::max (0, std::min (255, v)));
}

} // namespace

Vec3Df Display::map (const Vec3Df & linear) const {
    Vec3Df c = linear;
    if (exposure != 0.f)
        c *= std::exp2 (exposure);
    switch (toneMap) {
        case ToneMap::NONE:
            break;
        case ToneMap::REINHARD: {
            // On luminance, so the hue survives; channels may still pass 1
            // for saturated colours, and are clipped below.
            const float l = luminance (c);
            if (l > 0.f) {
                const float lw2 = whitePoint > 0.f ? whitePoint * whitePoint : 0.f;
                const float ld = lw2 > 0.f ? l * (1.f + l / lw2) / (1.f + l) : l / (1.f + l);
                c *= ld / l;
            }
            break;
        }
        case ToneMap::ACES:
            c = Vec3Df (aces (c[0]), aces (c[1]), aces (c[2]));
            break;
    }
    if (encoding == Encoding::LINEAR)
        return c;  // clamped by toBytes, exactly as before displays existed
    for (int i = 0; i < 3; ++i) {
        const float v = std::clamp (c[i], 0.f, 1.f);
        c[i] = encoding == Encoding::SRGB ? srgb (v) : std::pow (v, 1.f / gamma);
    }
    return c;
}

void Display::toBytes (const Vec3Df & linear, unsigned char rgb[3]) const {
    const Vec3Df c = map (linear);
    rgb[0] = toByte (c[0]);
    rgb[1] = toByte (c[1]);
    rgb[2] = toByte (c[2]);
}

Display Display::resolved (const HdrImage & hdr) const {
    Display d = *this;
    if (autoExposure) {
        d.exposure += meteredExposure (hdr);
        d.autoExposure = false;
    }
    return d;
}

Image Display::apply (const HdrImage & hdr) const {
    const Display d = resolved (hdr);
    Image image (hdr.width (), hdr.height (), Image::RGB888);
    unsigned char rgb[3];
    for (int y = 0; y < hdr.height (); ++y)
        for (int x = 0; x < hdr.width (); ++x) {
            d.toBytes (hdr.get (x, y), rgb);
            image.setPixel (x, y, rgb[0], rgb[1], rgb[2]);
        }
    return image;
}

float Display::meteredExposure (const HdrImage & hdr, float key) {
    double sumLog = 0.0;
    long count = 0;
    for (int y = 0; y < hdr.height (); ++y)
        for (int x = 0; x < hdr.width (); ++x) {
            const float l = luminance (hdr.get (x, y));
            if (l > 0.f) {
                sumLog += std::log (static_cast<double> (l));
                ++count;
            }
        }
    if (count == 0)
        return 0.f;
    const double logAverage = std::exp (sumLog / static_cast<double> (count));
    return static_cast<float> (std::log2 (key / logAverage));
}
