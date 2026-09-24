#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef WINVER
#define WINVER 0x0A00
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#include <windows.h>

static constexpr float kPortraitAspect = 9.f / 16.f;

static constexpr int kMinClientW = 405;
static constexpr int kMinClientH = 720;
static constexpr int kMaxClientW = 2160;
static constexpr int kMaxClientH = 3840;

static constexpr int kDefaultClientW = kMinClientW;
static constexpr int kDefaultClientH = kMinClientH;

struct AppWindow {
    HWND  hwnd = nullptr;
    HDC   hdc  = nullptr;
    HGLRC hrc  = nullptr;
    int   width  = kDefaultClientW;
    int   height = kDefaultClientH;
    bool  running = true;
    bool  displayChanged = false;
};

bool createAppWindow(AppWindow& w, const char* title, int clientW, int clientH);
void destroyAppWindow(AppWindow& w);
void pollAppWindow(AppWindow& w);
void swapAppWindow(AppWindow& w);
void applyHdrSafePresent();

void enforcePortraitSize(int& w, int& h);
