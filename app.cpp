#include "app.hpp"
#include "core/gl_loader.hpp"
#include "core/window.hpp"
#include "resource.h"
#include <cstdio>
#include <cstring>
#include <mmsystem.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>

static ISimpleAudioVolume* gSessionVol = nullptr;

static void releaseSessionVol() {
    if (gSessionVol) {
        gSessionVol->Release();
        gSessionVol = nullptr;
    }
}

static bool ensureSessionVol() {
    if (gSessionVol) return true;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {

    }

    IMMDeviceEnumerator* enumerator = nullptr;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator), (void**)&enumerator);
    if (FAILED(hr) || !enumerator) return false;

    IMMDevice* device = nullptr;
    hr = enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &device);
    enumerator->Release();
    if (FAILED(hr) || !device) return false;

    IAudioSessionManager* manager = nullptr;
    hr = device->Activate(__uuidof(IAudioSessionManager), CLSCTX_ALL,
                          nullptr, (void**)&manager);
    device->Release();
    if (FAILED(hr) || !manager) return false;

    hr = manager->GetSimpleAudioVolume(nullptr, 0, &gSessionVol);
    manager->Release();
    if (FAILED(hr) || !gSessionVol) {
        gSessionVol = nullptr;
        return false;
    }
    return true;
}

static constexpr float kWallStripe  = 0.69f;
static constexpr float kFloorStripe = 1.18f;
static constexpr float kDividerSpeed = 1.20f;

static constexpr float kCharHeight = 1.85f;
static constexpr float kCharZ      = 0.15f;
static constexpr float kCharX      = 0.f;
static constexpr float kCharY      = 0.f;
static constexpr int   kWalkFrames = 6;
static constexpr float kWalkFps    = 8.f;

static constexpr float kFlashPeriodSec = 0.35f;
static constexpr float kResultSec      = 5.0f;

static Vec3 hexRgb(unsigned rgb) {
    return Vec3(
        ((rgb >> 16) & 0xFF) / 255.f,
        ((rgb >>  8) & 0xFF) / 255.f,
        ( rgb        & 0xFF) / 255.f);
}

void App::updateTitle(int fps, float frameMs) {
    if (!window_.hwnd) return;

    int w = window_.width  > 0 ? window_.width  : 0;
    int h = window_.height > 0 ? window_.height : 0;

    char statusBuf[24];
    const char* status = statusBuf;
    if (flow_ == FlowState::ShowResult) {
        status = "[UNIVERSE]";
    } else if (flow_ == FlowState::FlashWait || pendingResolve_) {
        const int phase = ((int)(timer_.elapsed() / 0.5f) % 3 + 3) % 3;
        static const char* kThink[3] = {
            "[Thinking.]", "[Thinking..]", "[Thinking...]"
        };
        status = kThink[phase];
    } else {
        static const char kReadyRing[] = "---Ready---";
        static const int kReadyLen = 11;
        const int phase = ((int)(timer_.elapsed() / 0.5f) % kReadyLen + kReadyLen) % kReadyLen;
        char core[12];
        for (int i = 0; i < kReadyLen; ++i)
            core[i] = kReadyRing[(i + phase) % kReadyLen];
        core[kReadyLen] = '\0';
        std::snprintf(statusBuf, sizeof(statusBuf), "[%s]", core);
    }

    char title[256];
    std::snprintf(title, sizeof(title),
        "BrainChoice %s - %dx%d,%dbpp - %dFPS,%.2fms",
        status, w, h, colorBpp_, fps, frameMs);
    title[sizeof(title) - 1] = '\0';
    SetWindowTextA(window_.hwnd, title);
}

static void waitUntilQpc(LONGLONG targetTicks) {
    LARGE_INTEGER freq{}, now{};
    QueryPerformanceFrequency(&freq);
    for (;;) {
        QueryPerformanceCounter(&now);
        if (now.QuadPart >= targetTicks) return;
        const double left =
            (double)(targetTicks - now.QuadPart) / (double)freq.QuadPart;
        if (left > 0.002) {
            const DWORD ms = (DWORD)((left - 0.0015) * 1000.0);
            if (ms >= 1) Sleep(ms);
        } else {
            while (now.QuadPart < targetTicks)
                QueryPerformanceCounter(&now);
            return;
        }
    }
}

StripeColors App::flashPaletteLeft(bool altPhase) {
    if (!altPhase) return StripeColors::defaults();
    const Vec3 a = hexRgb(0x1F1AA6);
    const Vec3 b = hexRgb(0x011A46);
    StripeColors c;
    c.wallLight = a;
    c.wallDark  = b;
    c.floorLight = a;
    c.floorDark  = b;
    return c;
}

StripeColors App::flashPaletteRight(bool altPhase) {
    if (!altPhase) return StripeColors::defaults();
    const Vec3 a = hexRgb(0xF0398F);
    const Vec3 b = hexRgb(0xC039FF);
    StripeColors c;
    c.wallLight = a;
    c.wallDark  = b;
    c.floorLight = a;
    c.floorDark  = b;
    return c;
}

void App::localDayAndHourSlot(uint32_t& dayYmd, uint32_t& hourSlot) {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    dayYmd = (uint32_t)st.wYear * 10000u + (uint32_t)st.wMonth * 100u + (uint32_t)st.wDay;
    hourSlot = (uint32_t)st.wHour / 2u;
    if (hourSlot > 11u) hourSlot = 11u;
}

const wchar_t* App::wordText(FortuneWord w) {
    if (w == FortuneWord::Yes) return L"YES";
    if (w == FortuneWord::No) return L"NO";
    return L"PASS";
}

void App::onChatSendThunk(const std::string& text, void* user) {
    static_cast<App*>(user)->onChatSend(text);
}

void App::onMuteHotkeyThunk(void* user) {
    static_cast<App*>(user)->toggleBgmMute();
}

void App::toggleBgmMute() {
    if (!ensureSessionVol() || !gSessionVol) return;

    if (!bgmMuted_) {
        float cur = 1.f;
        if (SUCCEEDED(gSessionVol->GetMasterVolume(&cur)))
            bgmVolume_ = cur;

        gSessionVol->SetMute(TRUE, nullptr);
        bgmMuted_ = true;
    } else {
        gSessionVol->SetMute(FALSE, nullptr);
        if (bgmVolume_ < 0.f) bgmVolume_ = 0.f;
        if (bgmVolume_ > 1.f) bgmVolume_ = 1.f;
        gSessionVol->SetMasterVolume(bgmVolume_, nullptr);
        bgmMuted_ = false;
    }
}

void App::onChatSend(const std::string& text) {
    if (flow_ != FlowState::Idle || pendingResolve_) return;

    chatInput_.setInputAllowed(false);
    calcText_ = text;
    calcPos_ = 0;
    jiaSum_ = 0;
    fnvHash_ = 14695981039346656037ull;
    oddCount_ = 0;
    calcDone_ = false;
    sideKnown_ = false;
    flashLeft_ = true;
    pendingResolve_ = false;
    localDayAndHourSlot(dayYmd_, hourSlot_);

    if (calcText_.empty()) {
        verdict_.flashLeft = true;
        verdict_.word = FortuneWord::Pass;
        calcDone_ = true;
        beginResult(L"PASS", timer_.elapsed());
        return;
    }

    const size_t n = calcText_.size();
    if (n > 1000000) {
        calcText_.resize(1000000);
    }
    const size_t nn = calcText_.size();
    if (nn > 200000)
        calcChunk_ = 2048;
    else if (nn > 50000)
        calcChunk_ = 4096;
    else if (nn > 10000)
        calcChunk_ = 8192;
    else
        calcChunk_ = 16384;

    minThinkSec_ = fortuneMinThinkSec((uint64_t)nn);
    beginFlash();
}

void App::beginFlash() {
    flow_ = FlowState::FlashWait;
    flashStartSec_ = timer_.elapsed();
    flashAltPhase_ = false;
    flashPhaseTick_ = flashStartSec_;
    pendingResolve_ = false;
    updateTitle(lastFps_, lastFrameMs_);
}

void App::pumpCalculation(float nowSec) {
    if (calcDone_ || flow_ != FlowState::FlashWait) return;

    const size_t n = calcText_.size();
    size_t end = calcPos_ + calcChunk_;
    if (end > n) end = n;

    for (; calcPos_ < end; ++calcPos_) {
        const unsigned char c = (unsigned char)calcText_[calcPos_];
        const uint64_t next = jiaSum_ + (uint64_t)c;
        if (next < jiaSum_)
            jiaSum_ = (jiaSum_ % 1000000007ull) + (uint64_t)c;
        else
            jiaSum_ = next;
        fnvHash_ ^= (uint64_t)c;
        fnvHash_ *= 1099511628211ull;
        if (c & 1u) ++oddCount_;
    }

    if (calcPos_ >= n) {
        calcDone_ = true;
        verdict_ = sealFortune(jiaSum_, fnvHash_, (uint64_t)n, oddCount_, dayYmd_, hourSlot_);
        sideKnown_ = true;
        flashLeft_ = verdict_.flashLeft;
        flashAltPhase_ = true;
        flashPhaseTick_ = nowSec;
        calcText_.clear();
        calcText_.shrink_to_fit();
    }
}

void App::resolvePendingResult(float nowSec) {
    pendingResolve_ = false;
    beginResult(wordText(verdict_.word), nowSec);
}

void App::beginResult(const wchar_t* word, float nowSec) {
    flow_ = FlowState::ShowResult;
    resultStartSec_ = nowSec;
    resultDuration_ = kResultSec;
    resultBanner_.show(word);
    chatInput_.setInputAllowed(false);
    updateTitle(lastFps_, lastFrameMs_);
}

void App::endResult() {
    resultBanner_.hide();
    flow_ = FlowState::Idle;
    pendingResolve_ = false;
    calcDone_ = false;
    sideKnown_ = false;
    calcText_.clear();
    chatInput_.setInputAllowed(true);
    updateTitle(lastFps_, lastFrameMs_);
}

void App::updateFlow(float nowSec) {
    if (flow_ == FlowState::FlashWait) {
        pumpCalculation(nowSec);

        if (sideKnown_) {
            if (nowSec - flashPhaseTick_ >= kFlashPeriodSec) {
                flashPhaseTick_ = nowSec;
                flashAltPhase_ = !flashAltPhase_;
            }
        }

        if (!pendingResolve_ && calcDone_ &&
            nowSec - flashStartSec_ >= minThinkSec_) {
            flashAltPhase_ = false;
            pendingResolve_ = true;
        }
    } else if (flow_ == FlowState::ShowResult) {
        if (nowSec - resultStartSec_ >= resultDuration_) {
            endResult();
        }
    }
}

bool App::init(const LaunchSettings& settings) {
    modeHz_ = settings.refreshHz;
    if (modeHz_ < 1)    modeHz_ = 1;
    if (modeHz_ > 1000) modeHz_ = 1000;
    driverVsync_ = settings.vsync;
    colorBpp_ = settings.bpp > 0 ? settings.bpp : 32;

    int cw = settings.clientW;
    int ch = settings.clientH;
    enforcePortraitSize(cw, ch);

    if (!createAppWindow(window_, "BrainChoice", cw, ch)) {
        MessageBoxA(nullptr, "Window / OpenGL init failed.", "BrainChoice", MB_ICONERROR);
        return false;
    }

    updateTitle(0, 0.f);

    if (driverVsync_) {
        if (wglSwapIntervalEXT) wglSwapIntervalEXT(1);
    } else {
        if (wglSwapIntervalEXT) wglSwapIntervalEXT(0);
    }

    camera_.setup(Vec3(0.f, 1.55f, 3.2f), Vec3(0.f, 1.35f, -3.5f), 58.f);
    scroll_.setWallSpeed(1.65f);
    scroll_.setFloorSpeed(1.55f);

    auto failHard = [this](const char* msg) -> bool {
        MessageBoxA(window_.hwnd, msg, "BrainChoice", MB_ICONERROR);
        shutdown();
        return false;
    };

    if (!house_.init())
        return failHard("Scene init failed.");
    if (!divider_.init())
        return failHard("Divider init failed.");
    if (!CharacterSprite::initShared())
        return failHard("Character shader failed.");
    if (!charLeft_.loadSequence("assets\\alternate\\action\\B", kWalkFrames, kWalkFps, IDR_WALK_B_BASE))
        return failHard("Failed to load left character frames.");
    if (!charRight_.loadSequence("assets\\alternate\\action\\G", kWalkFrames, kWalkFps, IDR_WALK_G_BASE))
        return failHard("Failed to load right character frames.");

    if (!chatInput_.init(window_.hwnd)) {
        MessageBoxA(window_.hwnd, "Chat input bar failed to create.", "BrainChoice", MB_ICONWARNING);
    } else {
        chatInput_.setOnSend(&App::onChatSendThunk, this);
        chatInput_.setOnMuteHotkey(&App::onMuteHotkeyThunk, this);
        chatInput_.setInputAllowed(true);
    }

    if (!resultBanner_.init()) {
        MessageBoxA(window_.hwnd, "Result banner failed to create.", "BrainChoice", MB_ICONWARNING);
    }

    PlaySoundA(MAKEINTRESOURCEA(IDR_BGM), GetModuleHandleA(nullptr),
               SND_RESOURCE | SND_ASYNC | SND_LOOP | SND_NODEFAULT);
    bgmMuted_ = false;
    bgmVolume_ = 1.f;
    ensureSessionVol();

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glClearColor(0.f, 0.f, 0.f, 1.f);

    flow_ = FlowState::Idle;
    pendingResolve_ = false;
    return true;
}

void App::shutdown() {
    if (gSessionVol) {
        gSessionVol->SetMute(FALSE, nullptr);
        gSessionVol->SetMasterVolume(1.f, nullptr);
    }
    releaseSessionVol();
    PlaySoundA(nullptr, nullptr, 0);
    resultBanner_.destroy();
    chatInput_.destroy();
    charLeft_.destroy();
    charRight_.destroy();
    CharacterSprite::destroyShared();
    divider_.destroy();
    house_.destroy();
    destroyAppWindow(window_);
}

void App::render(float timeSec) {
    updateFlow(timeSec);

    RECT rc{};
    GetClientRect(window_.hwnd, &rc);
    int winW = rc.right - rc.left;
    int winH = rc.bottom - rc.top;
    if (winW < 1) winW = 1;
    if (winH < 1) winH = 1;
    window_.width  = winW;
    window_.height = winH;

    glViewport(0, 0, winW, winH);
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    int divW = winW / 160;
    if (divW < 1) divW = 1;
    if (divW > 2) divW = 2;

    const int leftW  = (winW - divW) / 2;
    const int rightW = winW - divW - leftW;
    const int leftX  = 0;
    const int divX   = leftW;
    const int rightX = divX + divW;
    const int vy     = 0;
    const int vh     = winH;

    const float halfAspect = (leftW > 0 && vh > 0)
        ? (float)leftW / (float)vh
        : 0.5f;

    const Mat4 proj = camera_.projMatrix(halfAspect);
    const Mat4 vp   = proj * camera_.viewMatrix();

    StripeColors leftCols  = StripeColors::defaults();
    StripeColors rightCols = StripeColors::defaults();
    if (flow_ == FlowState::FlashWait && sideKnown_) {
        if (flashLeft_)
            leftCols = flashPaletteLeft(flashAltPhase_);
        else
            rightCols = flashPaletteRight(flashAltPhase_);
    }

    glEnable(GL_SCISSOR_TEST);

    if (leftW > 0) {
        glViewport(leftX, vy, leftW, vh);
        glScissor(leftX, vy, leftW, vh);
        glClear(GL_DEPTH_BUFFER_BIT);
        house_.draw(vp, timeSec,
                    scroll_.wallSpeed(), scroll_.floorSpeed(),
                    kWallStripe, kFloorStripe, +1.f, leftCols);
        charLeft_.draw(vp, kCharX, kCharY, kCharZ, kCharHeight, timeSec);
    }

    if (rightW > 0) {
        glViewport(rightX, vy, rightW, vh);
        glScissor(rightX, vy, rightW, vh);
        glClear(GL_DEPTH_BUFFER_BIT);
        house_.draw(vp, timeSec,
                    scroll_.wallSpeed(), scroll_.floorSpeed(),
                    kWallStripe, kFloorStripe, -1.f, rightCols);
        charRight_.draw(vp, kCharX, kCharY, kCharZ, kCharHeight, timeSec);
    }

    glDisable(GL_SCISSOR_TEST);
    divider_.draw(divX, vy, divW, vh, timeSec, kDividerSpeed);

    if (resultBanner_.isVisible()) {
        glViewport(0, 0, winW, winH);
        resultBanner_.draw(winW, winH);
    }
}

int App::run() {
    const int capHz = (modeHz_ > 0) ? modeHz_ : 60;
    if (driverVsync_) timeBeginPeriod(1);

    if (wglSwapIntervalEXT) {
        if (!driverVsync_) wglSwapIntervalEXT(0);
        else if (capHz <= 60) wglSwapIntervalEXT(0);
        else wglSwapIntervalEXT(1);
    }

    LARGE_INTEGER qpcFreq{}, qpcNow{};
    QueryPerformanceFrequency(&qpcFreq);
    QueryPerformanceCounter(&qpcNow);

    const LONGLONG ticksPerFrame = driverVsync_
        ? (qpcFreq.QuadPart / (LONGLONG)capHz)
        : 0;
    LONGLONG nextDue = qpcNow.QuadPart;

    int fpsFrames = 0;
    float fpsWindowStart = timer_.elapsed();

    while (window_.running) {
        pollAppWindow(window_);

        if (pendingResolve_) {
            resolvePendingResult(timer_.elapsed());
        }

        render(timer_.elapsed());
        swapAppWindow(window_);

        if (driverVsync_ && ticksPerFrame > 0) {
            nextDue += ticksPerFrame;
            QueryPerformanceCounter(&qpcNow);
            if (qpcNow.QuadPart < nextDue) {
                waitUntilQpc(nextDue);
            } else {
                const LONGLONG late = qpcNow.QuadPart - nextDue;
                const LONGLONG skipped = late / ticksPerFrame;
                nextDue += (skipped + 1) * ticksPerFrame;
            }
        }

        ++fpsFrames;
        const float now = timer_.elapsed();
        const float dt = now - fpsWindowStart;
        if (dt >= 0.5f) {
            lastFps_ = (int)((float)fpsFrames / dt + 0.5f);
            lastFrameMs_ = (lastFps_ > 0)
                ? (1000.f / (float)lastFps_)
                : (dt * 1000.f / (float)(fpsFrames > 0 ? fpsFrames : 1));
            updateTitle(lastFps_, lastFrameMs_);
            fpsFrames = 0;
            fpsWindowStart = now;
        }
    }

    if (driverVsync_) timeEndPeriod(1);
    return 0;
}
