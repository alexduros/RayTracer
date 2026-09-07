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
    // Backdrops (the ground plane) are left out on purpose: the box drives
    // framing and light placement, which must follow the model itself.
    bool first = true;
    bbox = BoundingBox ();
    for (const Object & o : objects) {
        if (o.isBackdrop ())
            continue;
        if (first) {
            bbox = o.getBoundingBox ();
            first = false;
        } else {
            bbox.extendTo (o.getBoundingBox ());
        }
    }
}

void Scene::addGroundPlane (const Material & material, float extent) {
    const float size = std::max (bbox.getSize (), 1e-3f);
    const float h = extent * size;
    const Vec3Df c = bbox.getCenter ();
    const float y = bbox.getMin ()[1];
    const Vec3Df n (0.f, 1.f, 0.f);
    // Counter-clockwise seen from above (+Y), so the front face points up.
    const std::vector<Vertex> v = {
        Vertex (Vec3Df (c[0] - h, y, c[2] - h), n), Vertex (Vec3Df (c[0] - h, y, c[2] + h), n),
        Vertex (Vec3Df (c[0] + h, y, c[2] + h), n), Vertex (Vec3Df (c[0] + h, y, c[2] - h), n)};
    const std::vector<Triangle> t = {Triangle (0, 1, 2), Triangle (0, 2, 3)};
    Object ground (Mesh (v, t), material);
    ground.setBackdrop (true);
    objects.push_back (ground);  // a backdrop never changes bbox
}

void Scene::removeBackdrops () {
    objects.erase (std::remove_if (objects.begin (), objects.end (),
                                   [] (const Object & o) { return o.isBackdrop (); }),
                   objects.end ());
    updateBoundingBox ();
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
    // The original rig's colours and strengths (cyan key, yellow fill, white
    // rim), tuned for a model of size ~2 at the origin: scale by half the
    // bounding box size and re-centre. The fill and rim used to sit below the
    // model; they are raised above its bottom so a ground plane cannot block
    // them (every light stays above y = centre, hence above the floor).
    const Vec3Df c = bbox.getCenter ();
    const float s = std::max (bbox.getSize (), 1e-3f) / 2.f;
    lights.push_back (Light (c + s * Vec3Df (3.0f, 3.0f, 3.0f),  Vec3Df (0.0f, 1.0f, 1.0f), 1.0f, 3.0f * s));  // key
    lights.push_back (Light (c + s * Vec3Df (-3.0f, 1.0f, 2.0f), Vec3Df (1.0f, 1.0f, 0.0f), 0.5f, 3.0f * s));  // fill
    lights.push_back (Light (c + s * Vec3Df (0.0f, 2.0f, -3.0f), Vec3Df (1.0f, 1.0f, 1.0f), 0.8f, 3.0f * s));  // rim
}

void Scene::clear () {
    objects.clear ();
    lights.clear ();
    bbox = BoundingBox ();
}
