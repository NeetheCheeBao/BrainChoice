#include "scene/character_sprite.hpp"
#include <vector>
#include <cstdio>

Shader CharacterSprite::sShader_;
bool   CharacterSprite::sShaderOk_ = false;

static const char* kVS = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aColor;
layout(location=2) in vec2 aUv;
uniform mat4 uMVP;
out vec2 vUv;
void main(){
    vUv = aUv;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

static const char* kFS = R"(
#version 330 core
in vec2 vUv;
uniform sampler2D uTex;
out vec4 fragColor;
void main(){
    vec4 c = texture(uTex, vUv);
    if (c.a < 0.04) discard;
    fragColor = c;
}
)";

bool CharacterSprite::initShared() {
    if (sShaderOk_) return true;
    if (!sShader_.load(kVS, kFS)) {
        return false;
    }
    sShaderOk_ = true;
    return true;
}

void CharacterSprite::destroyShared() {
    sShader_.destroy();
    sShaderOk_ = false;
}

static void buildUnitQuad(Mesh& mesh) {
    std::vector<Vertex> verts;
    std::vector<unsigned> idx;
    const Vec3 n(0.f, 0.f, 0.f);
    verts.push_back({Vec3(-0.5f, 0.f, 0.f), n, Vec2(0.f, 0.f)});
    verts.push_back({Vec3( 0.5f, 0.f, 0.f), n, Vec2(1.f, 0.f)});
    verts.push_back({Vec3( 0.5f, 1.f, 0.f), n, Vec2(1.f, 1.f)});
    verts.push_back({Vec3(-0.5f, 1.f, 0.f), n, Vec2(0.f, 1.f)});
    idx = {0, 1, 2, 0, 2, 3};
    mesh.upload(verts, idx);
}

bool CharacterSprite::load(const char* pngPath) {
    destroy();
    if (!sShaderOk_ && !initShared()) return false;

    frames_.resize(1);
    if (!frames_[0].loadFromFile(pngPath)) {
        frames_.clear();
        return false;
    }
    buildUnitQuad(mesh_);
    fps_ = 0.f;
    return true;
}

bool CharacterSprite::loadSequence(const char* dir, int frameCount, float fps,
                                   int resIdBase) {
    destroy();
    if (!sShaderOk_ && !initShared()) return false;
    if (frameCount < 1) return false;

    frames_.resize((size_t)frameCount);

    if (resIdBase > 0) {
        bool allOk = true;
        for (int i = 0; i < frameCount; ++i) {
            if (!frames_[(size_t)i].loadFromResource(resIdBase + i)) {
                allOk = false;
                break;
            }
        }
        if (allOk) {
            buildUnitQuad(mesh_);
            fps_ = (fps > 0.f) ? fps : 8.f;
            return true;
        }
        for (auto& t : frames_) t.destroy();
    }

    if (!dir) {
        frames_.clear();
        return false;
    }

    char path[MAX_PATH];
    for (int i = 0; i < frameCount; ++i) {
        std::snprintf(path, sizeof(path), "%s%02d.png", dir, i);
        if (!frames_[(size_t)i].loadFromFile(path)) {
            destroy();
            char still[MAX_PATH];
            std::snprintf(still, sizeof(still), "%s.png", dir);
            return load(still);
        }
    }
    buildUnitQuad(mesh_);
    fps_ = (fps > 0.f) ? fps : 8.f;
    return true;
}

void CharacterSprite::destroy() {
    mesh_.destroy();
    for (auto& t : frames_) t.destroy();
    frames_.clear();
    fps_ = 8.f;
}

const Texture* CharacterSprite::frameAt(float timeSec) const {
    if (frames_.empty()) return nullptr;
    if (frames_.size() == 1 || fps_ <= 0.f) return &frames_[0];
    if (timeSec < 0.f) timeSec = 0.f;
    const int n = (int)frames_.size();
    int idx = (int)(timeSec * fps_);
    idx %= n;
    if (idx < 0) idx += n;
    return &frames_[(size_t)idx];
}

void CharacterSprite::draw(const Mat4& viewProj,
                           float x, float y, float z,
                           float heightWorld,
                           float timeSec) const {
    if (!ready() || !sShaderOk_ || heightWorld <= 0.f) return;
    const Texture* tex = frameAt(timeSec);
    if (!tex || !tex->id) return;

    const float w = heightWorld * tex->aspect();
    const Mat4 model =
        Mat4::translate(Vec3(x, y, z)) *
        Mat4::scale(Vec3(w, heightWorld, 1.f));
    const Mat4 mvp = viewProj * model;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_TRUE);

    sShader_.use();
    sShader_.setMat4("uMVP", mvp);
    sShader_.setInt("uTex", 0);
    tex->bind(0);
    mesh_.draw();

    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_BLEND);
}
