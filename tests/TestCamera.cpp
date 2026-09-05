#include <cmath>

#include "Camera.h"
#include "Fixtures.h"
#include "Test.h"

namespace {
const float kPi = 3.14159265358979f;
float angleBetween(const Vec3Df& a, const Vec3Df& b) {
    return std::acos(std::min(1.f, std::max(-1.f, Vec3Df::dotProduct(a, b) / (a.getLength() * b.getLength()))));
}
}  // namespace

TEST_CASE("camera: lookAt builds a right-handed orthonormal basis") {
    const Camera c = Camera::lookAt(Vec3Df(0.f, 0.f, 5.f), Vec3Df(0.f, 0.f, 0.f), Vec3Df(0.f, 1.f, 0.f), kPi / 4.f, 1.5f);
    CHECK(c.dir == Vec3Df(0.f, 0.f, -1.f));
    CHECK(c.up == Vec3Df(0.f, 1.f, 0.f));
    CHECK(c.right == Vec3Df(1.f, 0.f, 0.f));   // x to the right when looking down -z, like OpenGL
    CHECK_CLOSE(c.aspect, 1.5f, 0.f);

    // A tilted view: still orthonormal, still right-handed.
    const Camera t = Camera::lookAt(Vec3Df(3.f, 2.f, 4.f), Vec3Df(-1.f, 0.5f, 0.f), Vec3Df(0.f, 1.f, 0.f), kPi / 4.f, 1.f);
    CHECK_CLOSE(t.dir.getLength(), 1.f, 1e-5);
    CHECK_CLOSE(t.up.getLength(), 1.f, 1e-5);
    CHECK_CLOSE(t.right.getLength(), 1.f, 1e-5);
    CHECK_CLOSE(Vec3Df::dotProduct(t.dir, t.up), 0.f, 1e-5);
    CHECK_CLOSE(Vec3Df::dotProduct(t.dir, t.right), 0.f, 1e-5);
    CHECK_CLOSE(Vec3Df::dotProduct(t.up, t.right), 0.f, 1e-5);
    CHECK_CLOSE(Vec3Df::distance(Vec3Df::crossProduct(t.dir, t.up), t.right), 0.f, 1e-5);
    CHECK(t.up[1] > 0.f);   // world up stays up
}

TEST_CASE("camera: lookAt copes with a view along the up vector") {
    const Camera c = Camera::lookAt(Vec3Df(0.f, 5.f, 0.f), Vec3Df(0.f, 0.f, 0.f), Vec3Df(0.f, 1.f, 0.f), kPi / 4.f, 1.f);
    CHECK_CLOSE(c.right.getLength(), 1.f, 1e-5);
    CHECK_CLOSE(c.up.getLength(), 1.f, 1e-5);
    CHECK_CLOSE(Vec3Df::dotProduct(c.dir, c.right), 0.f, 1e-5);
}

TEST_CASE("camera: primary rays span exactly the field of view") {
    const float fovY = 60.f * kPi / 180.f;
    const float aspect = 2.f;
    const Camera c = Camera::lookAt(Vec3Df(0.f, 0.f, 0.f), Vec3Df(0.f, 0.f, -1.f), Vec3Df(0.f, 1.f, 0.f), fovY, aspect);
    const unsigned W = 200, H = 100;

    // Centre of the image: the view direction.
    CHECK_CLOSE(angleBetween(c.primaryRay(W / 2, H / 2, W, H, 0.f, 0.f).getDirection(), c.dir), 0.f, 1e-5);
    // Top edge, horizontally centred: half the vertical fov, pointing up.
    const Ray top = c.primaryRay(W / 2, 0, W, H, 0.f, 0.f);
    CHECK_CLOSE(angleBetween(top.getDirection(), c.dir), fovY / 2.f, 1e-4);
    CHECK(top.getDirection()[1] > 0.f);
    // Bottom edge: same angle, pointing down.
    CHECK_CLOSE(angleBetween(c.primaryRay(W / 2, H, W, H, 0.f, 0.f).getDirection(), c.dir), fovY / 2.f, 1e-4);
    // Right edge, vertically centred: tan(angle) = tan(fovY/2) * aspect, pointing +x.
    const Ray right = c.primaryRay(W, H / 2, W, H, 0.f, 0.f);
    CHECK_CLOSE(std::tan(angleBetween(right.getDirection(), c.dir)), std::tan(fovY / 2.f) * aspect, 1e-4);
    CHECK(right.getDirection()[0] > 0.f);
    // x = 0 is the left side of the image.
    CHECK(c.primaryRay(0, H / 2, W, H, 0.f, 0.f).getDirection()[0] < 0.f);
    // Every ray starts at the eye and is normalised.
    const Ray any = c.primaryRay(17, 63, W, H);
    CHECK(any.getOrigin() == c.pos);
    CHECK_CLOSE(any.getDirection().getLength(), 1.f, 1e-5);
}

TEST_CASE("camera: orbit and frame") {
    const Vec3Df target(1.f, 2.f, 3.f);
    const Camera front = Camera::orbit(target, 10.f, 0.f, 0.f, kPi / 4.f, 1.f);
    CHECK_CLOSE(Vec3Df::distance(front.pos, target + Vec3Df(0.f, 0.f, 10.f)), 0.f, 1e-4);
    CHECK_CLOSE(Vec3Df::distance(front.dir, Vec3Df(0.f, 0.f, -1.f)), 0.f, 1e-5);

    const Camera side = Camera::orbit(target, 10.f, 90.f, 0.f, kPi / 4.f, 1.f);
    CHECK_CLOSE(Vec3Df::distance(side.pos, target + Vec3Df(10.f, 0.f, 0.f)), 0.f, 1e-4);

    const Camera above = Camera::orbit(target, 10.f, 0.f, 90.f, kPi / 4.f, 1.f);   // clamped to 89
    CHECK(above.pos[1] > target[1] + 9.9f);
    CHECK_CLOSE(above.up.getLength(), 1.f, 1e-5);

    const BoundingBox box(Vec3Df(-1.f, -2.f, -3.f), Vec3Df(1.f, 2.f, 3.f));   // size 6
    const Camera framed = Camera::frame(box, kPi / 4.f, 1.f);
    CHECK_CLOSE(Vec3Df::distance(framed.pos, box.getCenter()), 12.f, 1e-4);   // 2 x size
    CHECK_CLOSE(Vec3Df::distance(framed.dir, Vec3Df(0.f, 0.f, -1.f)), 0.f, 1e-5);
}
