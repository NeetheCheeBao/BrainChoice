#pragma once
#include "gfx/mesh.hpp"
#include "gfx/shader.hpp"
#include "core/math.hpp"

struct StripeColors {
    Vec3 wallLight{1.00f, 0.90f, 0.28f};
    Vec3 wallDark {0.95f, 0.68f, 0.06f};
    Vec3 floorLight{0.82f, 0.82f, 0.84f};
    Vec3 floorDark {0.28f, 0.28f, 0.30f};

    static StripeColors defaults() { return {}; }
};

class StripedScene {
public:
    bool init();
    void destroy();

    void draw(const Mat4& viewProj,
              float timeSec,
              float wallSpeed, float floorSpeed,
              float wallStripe, float floorStripe,
              float scrollSign,
              const StripeColors& colors) const;

private:
    void buildHouseMesh();

    Mesh wallMesh_;
    Mesh floorMesh_;
    Shader houseShader_;
};
