#pragma once
#include "core/gl_loader.hpp"

struct Texture {
    GLuint id = 0;
    int    width  = 0;
    int    height = 0;

    bool loadFromFile(const char* path);

    bool loadFromResource(int resId);

    bool loadFromMemory(const void* data, size_t size, const char* debugName = nullptr);

    void bind(int unit = 0) const;
    void destroy();

    float aspect() const {
        return (height > 0) ? (float)width / (float)height : 1.f;
    }
};
