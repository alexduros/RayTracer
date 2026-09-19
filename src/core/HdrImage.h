// A floating-point RGB image: linear radiance as the tracer computes it,
// before exposure, tone mapping and 8-bit encoding (Display.h). Values above
// 1 are kept. Saved and loaded as Radiance .hdr files, the shared-exponent
// RGBE pixels of G. Ward, "Real Pixels", Graphics Gems II, 1991.
#ifndef HDRIMAGE_H
#define HDRIMAGE_H

#include <string>
#include <vector>

#include "Vec3D.h"

class HdrImage {
public:
    HdrImage () {}
    /// width x height pixels, all black.
    HdrImage (int width, int height)
        : w (width), h (height), rgb (static_cast<size_t> (width) * height * 3, 0.f) {}

    inline int width () const { return w; }
    inline int height () const { return h; }
    inline bool isValid () const { return w > 0 && h > 0; }

    inline Vec3Df get (int x, int y) const {
        const size_t i = index (x, y);
        return Vec3Df (rgb[i], rgb[i + 1], rgb[i + 2]);
    }
    inline void set (int x, int y, const Vec3Df & c) {
        const size_t i = index (x, y);
        rgb[i] = c[0];
        rgb[i + 1] = c[1];
        rgb[i + 2] = c[2];
    }
    void fill (const Vec3Df & c);
    /// Raw floats, three per pixel, rows top to bottom.
    inline const float * data () const { return rgb.data (); }
    inline float * data () { return rgb.data (); }

    /// Radiance RGBE (.hdr). False on failure.
    bool save (const std::string & filename) const;
    bool load (const std::string & filename);

private:
    inline size_t index (int x, int y) const { return (static_cast<size_t> (y) * w + x) * 3; }

    int w = 0, h = 0;
    std::vector<float> rgb;
};

#endif // HDRIMAGE_H
