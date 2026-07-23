#pragma once
#include "core/gl_loader.hpp"
#include "core/math.hpp"
#include <vector>

struct Vertex {
    Vec3 pos;
    Vec3 color;
    Vec2 uv;
};

struct Mesh {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLsizei indexCount = 0;

    void upload(const std::vector<Vertex>& verts, const std::vector<unsigned>& indices);
    void draw() const;
    void destroy();
};
