#include "scene/camera.hpp"
#include <cmath>

void FixedCamera::setup(const Vec3& eye, const Vec3& target, float fovyDeg) {
    eye_ = eye;
    target_ = target;
    fovy_ = fovyDeg;
}

Mat4 FixedCamera::viewMatrix() const {
    return Mat4::lookAt(eye_, target_, Vec3(0.f, 1.f, 0.f));
}

Mat4 FixedCamera::viewMatrixYawed(float yawDeg) const {
    const Vec3 forward = (target_ - eye_).normalized();
    const float rad = yawDeg * 3.14159265f / 180.f;
    const float c = std::cos(rad);
    const float s = std::sin(rad);

    const Vec3 f(
        forward.x * c + forward.z * s,
        forward.y,
       -forward.x * s + forward.z * c);
    return Mat4::lookAt(eye_, eye_ + f, Vec3(0.f, 1.f, 0.f));
}

Mat4 FixedCamera::projMatrix(float aspect) const {
    return Mat4::perspective(fovy_, aspect, 0.05f, 200.f);
}
