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

/// Which axis of a model file's coordinates points up (see Orientation.h).
enum class UpAxis { PosX, NegX, PosY, NegY, PosZ, NegZ };

/// Objects + lights. Plain value type: the GUI, the CLI and the tests each
/// build their own. The scene is Y-up: the floor is horizontal and the
/// camera orbits about Y.
class Scene {
public:
    Scene () {}

    /// Rotate every model object so that file axis `up` points along scene
    /// +Y (exact axis permutations, so switching back and forth is lossless).
    /// A ground plane, if present, is rebuilt under the new bottom. Lights
    /// are left alone: place them after orienting.
    void setUpAxis (UpAxis up);
    UpAxis getUpAxis () const { return upAxis; }

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

    /// A large quad at the bottom of the model's bounding box that receives
    /// its shadows and gives every render a floor. It is a *backdrop*: it
    /// does not count toward getBoundingBox(), so framing and the light rig
    /// keep following the model. `extent` is the half-size in multiples of
    /// the model's size.
    void addGroundPlane (const Material & material = groundMaterial (), float extent = 3.f);
    /// Remove backdrop objects (the ground plane).
    void removeBackdrops ();

    void clear ();

    static Material defaultMaterial () { return Material (1.f, 1.f, Vec3Df (1.f, .6f, .2f)); }
    static Material groundMaterial () { return Material (1.f, 0.f, Vec3Df (0.75f, 0.75f, 0.75f)); }

private:
    std::vector<Object> objects;
    std::vector<Light> lights;
    BoundingBox bbox;
    UpAxis upAxis = UpAxis::PosY;  // file axis currently mapped to scene +Y
};

#endif // SCENE_H
