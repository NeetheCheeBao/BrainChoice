#include "ui/result_banner.hpp"
#include "core/math.hpp"
#include <vector>
#include <cstring>
#include <windows.h>

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
uniform float uMode;
uniform float uAlpha;
out vec4 fragColor;
void main(){
    if (uMode < 0.5) {
        fragColor = vec4(0.08, 0.08, 0.12, uAlpha);
    } else {
        vec4 t = texture(uTex, vUv);
        fragColor = vec4(t.rgb, t.a * uAlpha);
    }
}
)";

ResultBanner::Kind ResultBanner::kindFromText(const wchar_t* text) {
    if (text && text[0] == L'Y') return Kind::Yes;
    if (text && text[0] == L'N') return Kind::No;
    return Kind::Pass;
}

bool ResultBanner::bakeOne(Kind kind, const wchar_t* text) {
    const int tw = 256;
    const int th = 64;

    COLORREF ink = RGB(255, 255, 255);
    if (kind == Kind::Yes)  ink = RGB(40, 220, 80);
    if (kind == Kind::No)   ink = RGB(240, 50, 50);
    if (kind == Kind::Pass) ink = RGB(255, 210, 40);

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = tw;
    bmi.bmiHeader.biHeight = th;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HDC hdcScreen = GetDC(nullptr);
    HDC hdc = CreateCompatibleDC(hdcScreen);
    HBITMAP hbmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hbmp || !bits) {
        if (hdc) DeleteDC(hdc);
        if (hdcScreen) ReleaseDC(nullptr, hdcScreen);
        return false;
    }

    HGDIOBJ oldBmp = SelectObject(hdc, hbmp);

    HBRUSH black = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RECT rc{0, 0, tw, th};
    FillRect(hdc, &rc, black);

    HFONT font = CreateFontW(
        -40, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Arial");
    HGDIOBJ oldFont = font ? SelectObject(hdc, font) : nullptr;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, ink);
    DrawTextW(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    const unsigned char tr = GetRValue(ink);
    const unsigned char tg = GetGValue(ink);
    const unsigned char tb = GetBValue(ink);

    std::vector<unsigned char> rgba((size_t)tw * th * 4);
    const unsigned char* src = (const unsigned char*)bits;
    for (int i = 0; i < tw * th; ++i) {
        const unsigned char b = src[i * 4 + 0];
        const unsigned char g = src[i * 4 + 1];
        const unsigned char r = src[i * 4 + 2];
        const unsigned char a = (r > g ? (r > b ? r : b) : (g > b ? g : b));
        rgba[i * 4 + 0] = tr;
        rgba[i * 4 + 1] = tg;
        rgba[i * 4 + 2] = tb;
        rgba[i * 4 + 3] = a;
    }

    if (oldFont) SelectObject(hdc, oldFont);
    if (font) DeleteObject(font);
    SelectObject(hdc, oldBmp);
    DeleteObject(hbmp);
    DeleteDC(hdc);
    ReleaseDC(nullptr, hdcScreen);

    const int idx = (int)kind;
    if (tex_[idx]) glDeleteTextures(1, &tex_[idx]);
    glGenTextures(1, &tex_[idx]);
    glBindTexture(GL_TEXTURE_2D, tex_[idx]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tw, th, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

bool ResultBanner::init() {
    destroy();
    if (!shader_.load(kVS, kFS)) return false;

    std::vector<Vertex> verts;
    std::vector<unsigned> idx;
    const Vec3 z(0, 0, 0);

    verts.push_back({Vec3(0, 0, 0), z, Vec2(0, 0)});
    verts.push_back({Vec3(1, 0, 0), z, Vec2(1, 0)});
    verts.push_back({Vec3(1, 1, 0), z, Vec2(1, 1)});
    verts.push_back({Vec3(0, 1, 0), z, Vec2(0, 1)});
    idx = {0, 1, 2, 0, 2, 3};
    quad_.upload(verts, idx);

    if (!bakeOne(Kind::Yes,  L"YES"))  return false;
    if (!bakeOne(Kind::No,   L"NO"))   return false;
    if (!bakeOne(Kind::Pass, L"PASS")) return false;

    ready_ = true;
    visible_ = false;
    kind_ = Kind::Pass;
    return true;
}

void ResultBanner::destroy() {
    for (int i = 0; i < (int)Kind::Count; ++i) {
        if (tex_[i]) {
            glDeleteTextures(1, &tex_[i]);
            tex_[i] = 0;
        }
    }
    quad_.destroy();
    shader_.destroy();
    visible_ = false;
    ready_ = false;
}

void ResultBanner::show(const wchar_t* text) {
    if (!ready_) return;
    kind_ = kindFromText(text);
    visible_ = true;
}

void ResultBanner::hide() {
    visible_ = false;
}

void ResultBanner::draw(int clientW, int clientH) {
    if (!ready_ || !visible_ || clientW < 1 || clientH < 1) return;

    const float W = (float)clientW;
    const float H = (float)clientH;

    const float marginX   = clampf(W * 0.04f, 14.f, 48.f);
    const float marginTop = clampf(H * 0.025f, 20.f, 64.f);
    const float barH      = clampf(H * 0.12f, 72.f, 420.f);
    const float barW      = W - marginX * 2.f;
    const float barX      = marginX;
    const float barY      = H - marginTop - barH;

    const Mat4 ortho = Mat4::ortho(0.f, W, 0.f, H, -1.f, 1.f);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    shader_.use();

    {
        const Mat4 model =
            Mat4::translate(Vec3(barX, barY, 0.f)) *
            Mat4::scale(Vec3(barW, barH, 1.f));
        shader_.setMat4("uMVP", ortho * model);
        shader_.setFloat("uMode", 0.f);
        shader_.setFloat("uAlpha", 0.78f);
        shader_.setInt("uTex", 0);
        quad_.draw();
    }

    const GLuint t = tex_[(int)kind_];
    if (t) {
        const float padX = barW * 0.10f;
        const float padY = barH * 0.15f;
        const float tw = barW - padX * 2.f;
        const float th = barH - padY * 2.f;
        const Mat4 model =
            Mat4::translate(Vec3(barX + padX, barY + padY, 0.f)) *
            Mat4::scale(Vec3(tw, th, 1.f));
        shader_.setMat4("uMVP", ortho * model);
        shader_.setFloat("uMode", 1.f);
        shader_.setFloat("uAlpha", 1.f);
        shader_.setInt("uTex", 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, t);
        quad_.draw();
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}
