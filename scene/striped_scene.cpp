#include "scene/striped_scene.hpp"
#include <vector>

static constexpr float kZWall  = -3.5f;
static constexpr float kZNear  = 80.0f;
static constexpr float kY0     =  0.0f;
static constexpr float kY1     = 200.0f;
static constexpr float kHalfW  = 500.0f;

static const char* kVS = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aColor;
layout(location=2) in vec2 aUv;
uniform mat4 uMVP;
out float vSurf;
out vec3  vPos;
void main(){
    vSurf = aColor.r;
    vPos  = aPos;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

static const char* kFS = R"(
#version 330 core
in float vSurf;
in vec3  vPos;
uniform float uTime;
uniform float uWallSpeed;
uniform float uFloorSpeed;
uniform float uStripeWall;
uniform float uStripeFloor;
uniform float uScrollSign;
uniform float uZWall;
uniform vec3  uWallLight;
uniform vec3  uWallDark;
uniform vec3  uFloorLight;
uniform vec3  uFloorDark;
out vec4 fragColor;

void main(){
    float id = vSurf;

    if (id > 2.5) {
        float shade = 0.55 + 0.15 * clamp((vPos.z - uZWall) * 0.08, 0.0, 1.0);
        fragColor = vec4(vec3(0.10, 0.09, 0.11) * shade, 1.0);
        return;
    }

    bool  isFloor = (id > 0.5 && id < 1.5);
    float S       = isFloor ? (-(vPos.z - uZWall)) : vPos.y;
    float stripe  = isFloor ? uStripeFloor : uStripeWall;
    float speed   = isFloor ? uFloorSpeed  : uWallSpeed;
    float phase   = S / max(stripe, 0.001) + uTime * speed * uScrollSign;
    float band    = fract(phase);

    vec3 lightC = isFloor ? uFloorLight : uWallLight;
    vec3 darkC  = isFloor ? uFloorDark  : uWallDark;
    fragColor = vec4((band < 0.5) ? lightC : darkC, 1.0);
}
)";

static void pushQuad(std::vector<Vertex>& v, std::vector<unsigned>& idx,
                     Vec3 p0, Vec3 p1, Vec3 p2, Vec3 p3, Vec3 id) {
    const unsigned b = (unsigned)v.size();
    const Vec2 z(0.f, 0.f);
    v.push_back({p0, id, z});
    v.push_back({p1, id, z});
    v.push_back({p2, id, z});
    v.push_back({p3, id, z});
    idx.push_back(b+0); idx.push_back(b+1); idx.push_back(b+2);
    idx.push_back(b+0); idx.push_back(b+2); idx.push_back(b+3);
}

bool StripedScene::init() {
    if (!houseShader_.load(kVS, kFS)) {
        return false;
    }
    buildHouseMesh();
    return true;
}

void StripedScene::buildHouseMesh() {
    const Vec3 backId (0.f, 0.f, 0.f);
    const Vec3 floorId(1.f, 0.f, 0.f);
    const Vec3 ceilId (3.f, 0.f, 0.f);

    const float x0 = -kHalfW, x1 = kHalfW;
    const float y0 = kY0, y1 = kY1;
    const float z0 = kZWall, z1 = kZNear;

    std::vector<Vertex> wallVerts, floorVerts;
    std::vector<unsigned> wallIdx, floorIdx;

    pushQuad(floorVerts, floorIdx,
             {x0, y0, z0}, {x1, y0, z0}, {x1, y0, z1}, {x0, y0, z1}, floorId);
    pushQuad(wallVerts, wallIdx,
             {x0, y0, z0}, {x1, y0, z0}, {x1, y1, z0}, {x0, y1, z0}, backId);
    pushQuad(wallVerts, wallIdx,
             {x0, y1, z0}, {x1, y1, z0}, {x1, y1, z1}, {x0, y1, z1}, ceilId);

    floorMesh_.upload(floorVerts, floorIdx);
    wallMesh_.upload(wallVerts, wallIdx);
}

void StripedScene::draw(const Mat4& viewProj,
                        float timeSec,
                        float wallSpeed, float floorSpeed,
                        float wallStripe, float floorStripe,
                        float scrollSign,
                        const StripeColors& colors) const {
    houseShader_.use();
    houseShader_.setMat4("uMVP", viewProj);
    houseShader_.setFloat("uTime", timeSec);
    houseShader_.setFloat("uWallSpeed", wallSpeed);
    houseShader_.setFloat("uFloorSpeed", floorSpeed);
    houseShader_.setFloat("uStripeWall", wallStripe);
    houseShader_.setFloat("uStripeFloor", floorStripe);
    houseShader_.setFloat("uScrollSign", scrollSign);
    houseShader_.setFloat("uZWall", kZWall);
    houseShader_.setVec3("uWallLight", colors.wallLight);
    houseShader_.setVec3("uWallDark", colors.wallDark);
    houseShader_.setVec3("uFloorLight", colors.floorLight);
    houseShader_.setVec3("uFloorDark", colors.floorDark);
    wallMesh_.draw();
    floorMesh_.draw();
}

void StripedScene::destroy() {
    wallMesh_.destroy();
    floorMesh_.destroy();
    houseShader_.destroy();
}
