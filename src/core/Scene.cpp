// *********************************************************
// Scene Class
// Author : Tamy Boubekeur (boubek@gmail.com).
// Copyright (C) 2010 Tamy Boubekeur.
// All rights reserved.
// *********************************************************

#include "Scene.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <stdexcept>

#include "ObjLoader.h"

void Scene::updateBoundingBox () {
    if (objects.empty ())
        bbox = BoundingBox ();
    else {
        bbox = objects[0].getBoundingBox ();
        for (unsigned int i = 1; i < objects.size (); i++)
            bbox.extendTo (objects[i].getBoundingBox ());
    }
}

void Scene::addObject (const Object & object) {
    objects.push_back (object);
    updateBoundingBox ();
}

void Scene::addLight (const Light & light) {
    lights.push_back (light);
}

void Scene::addObjectFromOFF (const std::string & filename, const Material & material) {
    Mesh mesh;
    mesh.loadOFF (filename);
    addObject (Object (mesh, material));
}

size_t Scene::addObjectsFromFile (const std::string & filename, const Material & material) {
    std::string ext = std::filesystem::path (filename).extension ().string ();
    std::transform (ext.begin (), ext.end (), ext.begin (), [] (unsigned char c) { return std::tolower (c); });
    if (ext == ".off") {
        addObjectFromOFF (filename, material);
        return 1;
    }
    if (ext == ".obj") {
        const std::vector<Object> objects = loadOBJ (filename, material);
        for (const Object & o : objects)
            addObject (o);
        return objects.size ();
    }
    throw std::runtime_error ("Scene: unsupported model format '" + ext + "' for " + filename +
                              " (expected .off or .obj)");
}

void Scene::addDefaultLights () {
    // Positions/radii below were tuned for a model of size ~2 centred at the
    // origin; scale them by half the bounding box size and re-centre.
    const Vec3Df c = bbox.getCenter ();
    const float s = std::max (bbox.getSize (), 1e-3f) / 2.f;
    lights.push_back (Light (c + s * Vec3Df (3.0f, 3.0f, 3.0f),   Vec3Df (0.0f, 1.0f, 1.0f), 1.0f, 3.0f * s));
    lights.push_back (Light (c + s * Vec3Df (-2.0f, -2.0f, 2.0f), Vec3Df (1.0f, 1.0f, 0.0f), 0.5f, 3.0f * s));
    lights.push_back (Light (c + s * Vec3Df (0.0f, -2.0f, 2.0f),  Vec3Df (1.0f, 1.0f, 1.0f), 0.8f, 3.0f * s));
}

void Scene::clear () {
    objects.clear ();
    lights.clear ();
    bbox = BoundingBox ();
}
