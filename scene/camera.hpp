#pragma once
#include "core/math.hpp"

class FixedCamera {
public:
    void setup(const Vec3& eye, const Vec3& target, float fovyDeg);

    Mat4 viewMatrix() const;

    Mat4 viewMatrixYawed(float yawDeg) const;
    Mat4 projMatrix(float aspect) const;

    const Vec3& eye() const { return eye_; }
    const Vec3& target() const { return target_; }

private:
    Vec3 eye_{0.f, 0.f, 8.f};
    Vec3 target_{0.f, 0.f, 0.f};
    float fovy_ = 50.f;
};
