#pragma once
#include "core/gl_loader.hpp"
#include "core/math.hpp"

struct Shader {
    GLuint id = 0;

    bool load(const char* vertexSrc, const char* fragmentSrc);
    void use() const;
    void destroy();

    void setMat4(const char* name, const Mat4& m) const;
    void setVec3(const char* name, const Vec3& v) const;
    void setVec4(const char* name, const Vec4& v) const;
    void setFloat(const char* name, float v) const;
    void setInt(const char* name, int v) const;
};
