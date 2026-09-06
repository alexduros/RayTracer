// *********************************************************
// Scene Class
// Author : Tamy Boubekeur (boubek@gmail.com).
// Copyright (C) 2010 Tamy Boubekeur.
// All rights reserved.
// *********************************************************

#ifndef SCENE_H
#define SCENE_H

#include <string>
#include <vector>

#include "Object.h"
#include "Light.h"
#include "BoundingBox.h"

/// Objects + lights. Plain value type: the GUI, the CLI and the tests each
/// build their own.
class Scene {
public:
    Scene () {}

    inline std::vector<Object> & getObjects () { return objects; }
    inline const std::vector<Object> & getObjects () const { return objects; }

    inline std::vector<Light> & getLights () { return lights; }
    inline const std::vector<Light> & getLights () const { return lights; }

    inline const BoundingBox & getBoundingBox () const { return bbox; }
    void updateBoundingBox ();

    void addObject (const Object & object);
    void addLight (const Light & light);

    /// Load an OFF file as one object. Throws std::runtime_error on failure.
    void addObjectFromOFF (const std::string & filename,
                           const Material & material = defaultMaterial ());

    /// Load any supported file by extension (case-insensitive): .off becomes
    /// one object with `material`; .obj becomes one object per material used,
    /// with `material` as the fallback (see ObjLoader.h). Returns the number
    /// of objects added. Throws std::runtime_error on failure or on an
    /// unsupported extension.
    size_t addObjectsFromFile (const std::string & filename,
                               const Material & material = defaultMaterial ());

    /// The original project's three-light rig (cyan key, yellow fill, white
    /// rim), scaled and centred on the current bounding box so it works for
    /// any model size. Call after the objects are added.
    void addDefaultLights ();

    void clear ();

    static Material defaultMaterial () { return Material (1.f, 1.f, Vec3Df (1.f, .6f, .2f)); }

private:
    std::vector<Object> objects;
    std::vector<Light> lights;
    BoundingBox bbox;
};

#endif // SCENE_H
