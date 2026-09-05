#include "Camera.h"

#include <algorithm>
#include <cmath>

namespace {
const float kDegToRad = 3.14159265358979f / 180.f;
}

Camera Camera::lookAt (const Vec3Df & pos, const Vec3Df & target, const Vec3Df & worldUp,
                       float fovY, float aspect) {
    Camera c;
    c.pos = pos;
    c.dir = target - pos;
    c.dir.normalize ();
    c.right = Vec3Df::crossProduct (c.dir, worldUp);
    if (c.right.getSquaredLength () < 1e-12f) {
        // Looking straight along worldUp: any perpendicular axis will do.
        Vec3Df u, v;
        c.dir.getTwoOrthogonals (u, v);
        c.right = u;
    }
    c.right.normalize ();
    c.up = Vec3Df::crossProduct (c.right, c.dir);
    c.up.normalize ();
    c.fovY = fovY;
    c.aspect = aspect;
    return c;
}

Camera Camera::orbit (const Vec3Df & target, float distance, float yawDeg, float pitchDeg,
                      float fovY, float aspect) {
    const float yaw = yawDeg * kDegToRad;
    const float pitch = std::clamp (pitchDeg, -89.f, 89.f) * kDegToRad;
    const Vec3Df offset (distance * std::cos (pitch) * std::sin (yaw),
                         distance * std::sin (pitch),
                         distance * std::cos (pitch) * std::cos (yaw));
    return lookAt (target + offset, target, Vec3Df (0.f, 1.f, 0.f), fovY, aspect);
}

Camera Camera::frame (const BoundingBox & bbox, float fovY, float aspect,
                      float distanceFactor, float yawDeg, float pitchDeg) {
    const float size = std::max (bbox.getSize (), 1e-3f);
    return orbit (bbox.getCenter (), distanceFactor * size, yawDeg, pitchDeg, fovY, aspect);
}

Ray Camera::primaryRay (unsigned int x, unsigned int y, unsigned int width, unsigned int height,
                        float sx, float sy) const {
    const float tanHalf = std::tan (fovY * 0.5f);
    const float ndcX = ((x + sx) / float (width)) * 2.f - 1.f;   // -1 (left) .. +1 (right)
    const float ndcY = 1.f - ((y + sy) / float (height)) * 2.f;  // +1 (top)  .. -1 (bottom)
    Vec3Df d = dir + right * (ndcX * tanHalf * aspect) + up * (ndcY * tanHalf);
    d.normalize ();
    return Ray (pos, d);
}
