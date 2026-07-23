#pragma once
#include "core/window.hpp"
#include "core/timer.hpp"
#include "core/big_int.hpp"
#include "scene/camera.hpp"
#include "scene/scroll_controller.hpp"
#include "scene/striped_scene.hpp"
#include "scene/center_divider.hpp"
#include "scene/character_sprite.hpp"
#include "ui/launcher.hpp"
#include "ui/chat_input.hpp"
#include "ui/result_banner.hpp"
#include <string>

class App {
public:
    bool init(const LaunchSettings& settings);
    void shutdown();
    int  run();

private:
    enum class FlowState {
        Idle,
        FlashWait,
        ShowResult
    };

    void render(float timeSec);
    void updateTitle(int fps, float frameMs);
    void updateFlow(float nowSec);
    void pumpCalculation(float nowSec);
    void resolvePendingResult(float nowSec);

    void onChatSend(const std::string& text);
    static void onChatSendThunk(const std::string& text, void* user);

    void toggleBgmMute();
    static void onMuteHotkeyThunk(void* user);

    void beginFlash();
    void beginResult(const wchar_t* word, float nowSec);
    void endResult();

    static void localDayAndHourSlot(uint32_t& dayYmd, uint32_t& hourSlot);
    static StripeColors flashPaletteLeft(bool altPhase);
    static StripeColors flashPaletteRight(bool altPhase);
    static const wchar_t* wordText(FortuneWord w);

    AppWindow window_;
    Timer timer_;

    FixedCamera camera_;
    ScrollController scroll_;
    StripedScene house_;
    CenterDivider divider_;
    CharacterSprite charLeft_;
    CharacterSprite charRight_;
    ChatInput chatInput_;
    ResultBanner resultBanner_;

    bool driverVsync_ = true;
    int  modeHz_      = 60;
    int  colorBpp_    = 32;

    int   lastFps_ = 0;
    float lastFrameMs_ = 0.f;

    FlowState flow_ = FlowState::Idle;
    FortuneVerdict verdict_{};
    bool      flashLeft_ = true;
    bool      sideKnown_ = false;
    float     flashStartSec_ = 0.f;
    float     minThinkSec_ = 2.f;
    float     resultStartSec_ = 0.f;
    float     resultDuration_ = 5.f;
    bool      flashAltPhase_ = false;
    float     flashPhaseTick_ = 0.f;
    bool      pendingResolve_ = false;

    std::string calcText_;
    size_t calcPos_ = 0;
    uint64_t jiaSum_ = 0;
    uint64_t fnvHash_ = 14695981039346656037ull;
    uint64_t oddCount_ = 0;
    uint32_t dayYmd_ = 0;
    uint32_t hourSlot_ = 0;
    bool calcDone_ = false;
    size_t calcChunk_ = 4096;

    bool bgmMuted_ = false;
    float bgmVolume_ = 1.f;
};
