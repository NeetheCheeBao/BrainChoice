#pragma once
#include "gfx/shader.hpp"

class CenterDivider {
public:
    bool init();
    void destroy();

    void draw(int vx, int vy, int vw, int vh, float timeSec, float speed) const;

private:
    Shader shader_;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
};
