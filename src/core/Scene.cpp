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

namespace {

/// Integer rotation matrices (rows = scene axes) taking file coordinates to
/// scene coordinates so that the given file axis lands on scene +Y. Exact,
/// proper rotations (determinant +1).
typedef int Mat3[3][3];

void rotationFor (UpAxis up, Mat3 m) {
    static const Mat3 kPosY = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    static const Mat3 kPosZ = {{1, 0, 0}, {0, 0, 1}, {0, -1, 0}};   // (x, y, z) -> (x, z, -y)
    static const Mat3 kNegZ = {{1, 0, 0}, {0, 0, -1}, {0, 1, 0}};   // (x, y, z) -> (x, -z, y)
    static const Mat3 kPosX = {{0, -1, 0}, {1, 0, 0}, {0, 0, 1}};   // (x, y, z) -> (-y, x, z)
    static const Mat3 kNegX = {{0, 1, 0}, {-1, 0, 0}, {0, 0, 1}};   // (x, y, z) -> (y, -x, z)
    static const Mat3 kNegY = {{1, 0, 0}, {0, -1, 0}, {0, 0, -1}};  // (x, y, z) -> (x, -y, -z)
    const int (*src)[3] = kPosY;
    switch (up) {
        case UpAxis::PosY: src = kPosY; break;
        case UpAxis::PosZ: src = kPosZ; break;
        case UpAxis::NegZ: src = kNegZ; break;
        case UpAxis::PosX: src = kPosX; break;
        case UpAxis::NegX: src = kNegX; break;
        case UpAxis::NegY: src = kNegY; break;
    }
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            m[r][c] = src[r][c];
}

Vec3Df apply (const Mat3 m, const Vec3Df & v) {
    return Vec3Df (m[0][0] * v[0] + m[0][1] * v[1] + m[0][2] * v[2],
                   m[1][0] * v[0] + m[1][1] * v[1] + m[1][2] * v[2],
                   m[2][0] * v[0] + m[2][1] * v[1] + m[2][2] * v[2]);
}

} // namespace

void Scene::setUpAxis (UpAxis up) {
    if (up == upAxis)
        return;
    // Scene coordinates are s = M_current * f. To reach s' = M_new * f apply
    // M_new * M_current^T, which is another exact permutation.
    Mat3 current, target, transform;
    rotationFor (upAxis, current);
    rotationFor (up, target);
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c) {
            transform[r][c] = 0;
            for (int k = 0; k < 3; ++k)
                transform[r][c] += target[r][k] * current[c][k];  // current transposed
        }

    // Remember the ground (a backdrop) so it can be rebuilt under the new bottom.
    bool hadGround = false;
    Material groundMat;
    for (const Object & o : objects)
        if (o.isBackdrop ()) {
            hadGround = true;
            groundMat = o.getMaterial ();
        }
    removeBackdrops ();

    for (Object & o : objects) {
        for (Vertex & v : o.getMesh ().getVertices ()) {
            v.setPos (apply (transform, v.getPos ()));
            v.setNormal (apply (transform, v.getNormal ()));
        }
        o.updateBoundingBox ();
    }
    upAxis = up;
    updateBoundingBox ();
    if (hadGround)
        addGroundPlane (groundMat);
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
