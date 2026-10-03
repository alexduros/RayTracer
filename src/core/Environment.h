// The world around the scene, as one picture indexed by direction: a ray
// that leaves the scene reads the picture where it points, so mirrors and
// glass reflect a place instead of a flat background.
//
// J. F. Blinn & M. E. Newell, "Texture and Reflection in Computer Generated
// Images", Communications of the ACM 19(10), 1976: "the reflection
// direction, instead of parametric surface position, determines the
// coordinates in the map", plotted "with azimuthal angle as abscissa and
// polar angle as ordinate".
//
// The picture is a latitude-longitude panorama, +Y up like the scene: the
// azimuth atan2 (x, -z) runs across it and the polar angle acos (y) down it.
// Its middle is the direction -Z, where the default camera looks, +X is a
// quarter of the width to the right, and its two side edges meet behind the
// camera. Only the direction counts, never the point the ray left from: as
// the paper says, that puts the world on an infinitely large sphere.
#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include <memory>
#include <string>

#include "Texture.h"
#include "Vec3D.h"

class Environment {
public:
    /// Reads a panorama: a Radiance .hdr, taken as the linear radiance it
    /// holds (above 1 included), or any format stb decodes, taken as sRGB.
    /// Returns nullptr and warns when the file cannot be read.
    static std::shared_ptr<const Environment> load (const std::string & filename);

    /// `map` must not be null.
    explicit Environment (const std::shared_ptr<const Texture> & map) : map (map) {}

    /// Where a direction (any length) falls in the picture, both in [0, 1]:
    /// `u` across, from the left edge, and `v` down, from the top row.
    static void toMap (const Vec3Df & direction, float & u, float & v);
    /// The unit direction of a point of the picture: the way back from toMap.
    static Vec3Df toDirection (float u, float v);

    /// Radiance arriving from `direction` (any length), bilinear between the
    /// four texels around it. The picture closes on itself in longitude; its
    /// top and bottom rows do not mix.
    Vec3Df sample (const Vec3Df & direction) const;

    int width () const { return map->width (); }
    int height () const { return map->height (); }
    const std::string & path () const { return map->path (); }

private:
    std::shared_ptr<const Texture> map;  // shared and never modified
};

#endif // ENVIRONMENT_H
