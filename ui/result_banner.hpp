#pragma once
#include "core/gl_loader.hpp"
#include "gfx/shader.hpp"
#include "gfx/mesh.hpp"

class ResultBanner {
public:
    bool init();
    void destroy();

    void show(const wchar_t* text);
    void hide();
    bool isVisible() const { return visible_; }

    void draw(int clientW, int clientH);

private:
    enum class Kind { Yes = 0, No = 1, Pass = 2, Count = 3 };

    bool bakeOne(Kind kind, const wchar_t* text);
    static Kind kindFromText(const wchar_t* text);

    Shader shader_;
    Mesh   quad_;
    GLuint tex_[(int)Kind::Count] = {};
    Kind   kind_ = Kind::Pass;
    bool   visible_ = false;
    bool   ready_ = false;
};
