#ifndef CAMERA_H
#define CAMERA_H

#include "Vec3D.h"
#include "BoundingBox.h"
#include "Ray.h"

/// Pinhole camera, or a thin lens. (dir, up, right) is an orthonormal basis,
/// fovY is the vertical field of view in radians and aspect is width /
/// height. This is the same convention as glm::lookAt + glm::perspective in
/// the GUI, so a raytraced frame lines up with the GL preview.
///
/// With an aperture, the camera is a thin lens of that radius around `pos`,
/// in the plane spanned by right and up, focused on the plane at
/// `focusDistance` along dir (Potmesil & Chakravarty 1981). Every ray of a
/// pixel crosses that plane where the pinhole ray does, from its own point
/// of the lens: what lies on the plane stays sharp, anything nearer or
/// farther spreads over a disk, the circle of confusion. The GL preview
/// stays a pinhole.
///
/// Pixel (0, 0) is the top-left corner of the image, matching Image and PNG
/// row order.
struct Camera {
    Vec3Df pos   = Vec3Df (0.f, 0.f, 0.f);
    Vec3Df dir   = Vec3Df (0.f, 0.f, -1.f);
    Vec3Df up    = Vec3Df (0.f, 1.f, 0.f);
    Vec3Df right = Vec3Df (1.f, 0.f, 0.f);
    float fovY   = 45.f * 3.14159265358979f / 180.f;
    float aspect = 1.f;
    float aperture = 0.f;       // radius of the lens in world units; 0 = pinhole
    float focusDistance = 1.f;  // from pos to the plane in focus, along dir

    /// Camera at `pos` looking at `target`, focused on it; `worldUp` is
    /// re-orthogonalised.
    static Camera lookAt (const Vec3Df & pos, const Vec3Df & target, const Vec3Df & worldUp,
                          float fovY, float aspect);

    /// Camera on a sphere of radius `distance` around `target`. yaw rotates
    /// about +Y, pitch is the elevation (clamped to +-89 degrees), both in
    /// degrees. yaw = pitch = 0 puts the camera on +Z looking down -Z, which
    /// is how the GUI frames a freshly loaded model.
    static Camera orbit (const Vec3Df & target, float distance, float yawDeg, float pitchDeg,
                         float fovY, float aspect);

    /// orbit() around the box centre at distanceFactor * bbox.getSize().
    static Camera frame (const BoundingBox & bbox, float fovY, float aspect,
                         float distanceFactor = 2.f, float yawDeg = 0.f, float pitchDeg = 0.f);

    /// Primary ray through pixel (x, y) at sub-pixel offset (sx, sy) in [0, 1),
    /// leaving the lens at (lx, ly) of the unit disk (x along right, y along
    /// up). Without an aperture the lens point is ignored: the pinhole ray.
    Ray primaryRay (unsigned int x, unsigned int y, unsigned int width, unsigned int height,
                    float sx = 0.5f, float sy = 0.5f, float lx = 0.f, float ly = 0.f) const;
};

#endif // CAMERA_H
