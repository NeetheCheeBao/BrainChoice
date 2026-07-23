#include "scene/center_divider.hpp"
#include "core/math.hpp"

static const char* kVS = R"(
#version 330 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUv;
uniform mat4 uMVP;
out vec2 vUv;
void main(){
    vUv = aUv;
    gl_Position = uMVP * vec4(aPos, 0.0, 1.0);
}
)";

static const char* kFS = R"(
#version 330 core
in vec2 vUv;
uniform float uTime;
uniform float uSpeed;
uniform float uBands;
out vec4 fragColor;
void main(){
    float phase = vUv.y * uBands + uTime * uSpeed;
    float band = fract(phase);
    vec3 col = (band < 0.5) ? vec3(0.92, 0.92, 0.92) : vec3(0.06, 0.06, 0.06);
    fragColor = vec4(col, 1.0);
}
)";

bool CenterDivider::init() {
    if (!shader_.load(kVS, kFS)) {
        return false;
    }

    const float verts[] = {
        0.f, 0.f,  0.f, 0.f,
        1.f, 0.f,  1.f, 0.f,
        1.f, 1.f,  1.f, 1.f,
        0.f, 0.f,  0.f, 0.f,
        1.f, 1.f,  1.f, 1.f,
        0.f, 1.f,  0.f, 1.f,
    };

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
    return true;
}

void CenterDivider::destroy() {
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    vbo_ = vao_ = 0;
    shader_.destroy();
}

void CenterDivider::draw(int vx, int vy, int vw, int vh, float timeSec, float speed) const {
    if (vw < 1 || vh < 1) return;

    glViewport(vx, vy, vw, vh);
    glDisable(GL_DEPTH_TEST);

    Mat4 mvp = Mat4::ortho(0.f, 1.f, 0.f, 1.f, -1.f, 1.f);
    const float bands = (float)vh / 72.f;

    shader_.use();
    shader_.setMat4("uMVP", mvp);
    shader_.setFloat("uTime", timeSec);
    shader_.setFloat("uSpeed", speed);
    shader_.setFloat("uBands", bands);

    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
}
