// An image read through texture coordinates: the surface carries its own
// (u, v) frame, and the colour comes from a picture rather than a constant.
//
// E. Catmull, "A Subdivision Algorithm for Computer Display of Curved
// Surfaces", PhD thesis, University of Utah, 1974, chapter 6.
//
// Texels are decoded from sRGB to linear radiance once, at load: everything
// the tracer computes is linear, and the display encodes it back at the end
// (Display.h). A Radiance .hdr file holds linear radiance already and is
// taken as it is, above 1 included. Sampling is bilinear, and coordinates
// outside [0, 1] wrap, so a texture tiles.
#ifndef TEXTURE_H
#define TEXTURE_H

#include <memory>
#include <string>
#include <vector>

#include "Vec3D.h"

class Texture {
public:
    /// Reads any format stb decodes (PNG, JPEG, TGA, Radiance .hdr...).
    /// Returns nullptr and warns when the file cannot be read; the caller
    /// then keeps its colour.
    static std::shared_ptr<const Texture> load (const std::string & filename);
    /// The same without the warning about a material: for pictures that are
    /// not a surface's colour (Environment.h), whose caller says what failed.
    static std::shared_ptr<const Texture> read (const std::string & filename);

    /// Bilinear sample in linear radiance. Coordinates wrap: 1.25 reads 0.25,
    /// -0.25 reads 0.75. v = 0 is the bottom of the image, as OBJ files mean it.
    Vec3Df sample (float u, float v) const;

    int width () const { return w; }
    int height () const { return h; }
    const std::string & path () const { return name; }

private:
    Texture (int width, int height, std::vector<Vec3Df> && texels, const std::string & name)
        : w (width), h (height), texels (std::move (texels)), name (name) {}

    const Vec3Df & texel (int x, int y) const;

    int w = 0, h = 0;
    std::vector<Vec3Df> texels;  // linear, row 0 at the top of the image
    std::string name;
};

#endif // TEXTURE_H
