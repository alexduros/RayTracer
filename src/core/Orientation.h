// Which way is up. OFF and OBJ files carry no convention: some of the bundled
// models are Z-up (teapot, ram), others Y-up (minion). The scene itself is
// Y-up (the floor is horizontal, the camera orbits about Y), so a model is
// rotated on load so that its own up axis becomes +Y (Scene::setUpAxis).
//
// "Auto" resolves the axis from an `orientation.txt` next to the model file
// when it has an entry, and otherwise from a heuristic that is right for
// most models with a flat base and wrong for a few (anything with a flat
// back), which is why the viewer and the CLI let you override it.
#ifndef ORIENTATION_H
#define ORIENTATION_H

#include <optional>
#include <string>

#include "Scene.h"

const char * upAxisName (UpAxis up);                        // "+Y", "-Z", ...
std::optional<UpAxis> parseUpAxis (const std::string & s);  // "+y", "Y", "-z", "z" ...

/// "The flattest side is the bottom": the face of the model's bounding box
/// carrying the most vertices (within 1% of the extent) is taken as the
/// ground side; up is the opposite direction. Ties prefer +Y, then +Z.
/// Must run on the file's coordinates, i.e. before Scene::setUpAxis.
UpAxis detectUpAxis (const Scene & scene);

/// Curated orientation from `orientation.txt` in the model's directory: one
/// "name axis" pair per line, name without extension (e.g. "ram +z"),
/// '#' starts a comment. Empty when there is no file or no entry.
std::optional<UpAxis> upAxisFromManifest (const std::string & modelPath);

/// Manifest entry if any, else the heuristic. `source` receives
/// "orientation.txt" or "heuristic" when given.
UpAxis resolveUpAxis (const std::string & modelPath, const Scene & scene, std::string * source = nullptr);

#endif // ORIENTATION_H
