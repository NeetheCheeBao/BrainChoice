#pragma once
#include "gfx/mesh.hpp"
#include "gfx/shader.hpp"
#include "gfx/texture.hpp"
#include "core/math.hpp"
#include <vector>

class CharacterSprite {
public:

    static bool initShared();
    static void destroyShared();

    bool load(const char* pngPath);

    bool loadSequence(const char* dir, int frameCount, float fps,
                      int resIdBase = 0);

    void destroy();

    void draw(const Mat4& viewProj,
              float x, float y, float z,
              float heightWorld,
              float timeSec = 0.f) const;

    bool ready() const { return !frames_.empty() && mesh_.vao != 0; }
    float aspect() const {
        return frames_.empty() ? 1.f : frames_[0].aspect();
    }
    int frameCount() const { return (int)frames_.size(); }

private:
    const Texture* frameAt(float timeSec) const;

    std::vector<Texture> frames_;
    Mesh  mesh_;
    float fps_ = 8.f;

    static Shader sShader_;
    static bool   sShaderOk_;
};
