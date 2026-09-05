#include "Test.h"
#include "Vec3D.h"

TEST_CASE("vec3d: dot, cross, length") {
    const Vec3Df x(1.f, 0.f, 0.f), y(0.f, 1.f, 0.f), z(0.f, 0.f, 1.f);
    CHECK_CLOSE(Vec3Df::dotProduct(x, y), 0.f, 1e-6);
    CHECK_CLOSE(Vec3Df::dotProduct(x, x), 1.f, 1e-6);
    CHECK(Vec3Df::crossProduct(x, y) == z);
    CHECK(Vec3Df::crossProduct(y, x) == -z);
    CHECK_CLOSE(Vec3Df(3.f, 4.f, 0.f).getLength(), 5.f, 1e-6);
    CHECK_CLOSE(Vec3Df::distance(Vec3Df(1.f, 2.f, 3.f), Vec3Df(1.f, 2.f, 3.f)), 0.f, 1e-6);
}

TEST_CASE("vec3d: normalize returns the old length and handles zero") {
    // normalize() mutates and returns the previous length; capture it before
    // asserting so the value is read exactly once.
    Vec3Df v(0.f, 3.f, 4.f);
    const float length = v.normalize();
    CHECK_CLOSE(length, 5.f, 1e-6);
    CHECK_CLOSE(v.getLength(), 1.f, 1e-6);
    Vec3Df zero;
    const float zeroLength = zero.normalize();
    CHECK_CLOSE(zeroLength, 0.f, 1e-6);
    CHECK(zero == Vec3Df(0.f, 0.f, 0.f));
}

TEST_CASE("vec3d: arithmetic operators") {
    const Vec3Df a(1.f, 2.f, 3.f), b(4.f, 5.f, 6.f);
    CHECK(a + b == Vec3Df(5.f, 7.f, 9.f));
    CHECK(b - a == Vec3Df(3.f, 3.f, 3.f));
    CHECK(a * 2.f == Vec3Df(2.f, 4.f, 6.f));
    CHECK(2.f * a == Vec3Df(2.f, 4.f, 6.f));
    CHECK(a * b == Vec3Df(4.f, 10.f, 18.f));   // component-wise
    CHECK(b / 2.f == Vec3Df(2.f, 2.5f, 3.f));
    Vec3Df c = a;
    c += b;
    CHECK(c == Vec3Df(5.f, 7.f, 9.f));
    CHECK(Vec3Df::interpolate(a, b, 0.5f) == Vec3Df(2.5f, 3.5f, 4.5f));
}
