// Wavefront OBJ (+ MTL) loader.
//
// An OBJ file can switch materials per face while the scene model is "one
// Mesh + one Material per Object", so a file is split into one Object per
// material used, in first-use order (faces of the same material in separate
// runs merge into the same object). Vertices are de-duplicated per object on
// (position, normal). Texture coordinates are parsed for validation but not
// kept: there are no textures yet. Normals from the file are honoured; if any
// face corner of an object lacks one, smooth normals are recomputed for that
// object.
#ifndef OBJLOADER_H
#define OBJLOADER_H

#include <map>
#include <string>
#include <vector>

#include "Material.h"
#include "Object.h"

/// Materials of an MTL file by name. Kd -> colour, mean(Ks) -> specular,
/// Ns -> shininess; everything else (Ka, d, illum, map_*) is ignored for
/// now. A material without Kd is light grey. Throws std::runtime_error if
/// the file cannot be opened.
std::map<std::string, Material> loadMTL (const std::string & filename);

/// One Object per material used by `filename`. `mtllib` paths resolve
/// relative to the OBJ's directory; a missing MTL or an unknown material name
/// falls back to `fallback` with a warning on stderr. Throws
/// std::runtime_error on a missing or malformed OBJ, or one with no faces.
std::vector<Object> loadOBJ (const std::string & filename, const Material & fallback);

#endif // OBJLOADER_H
