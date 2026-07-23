#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>

class ChatInput {
public:
    using SendCallback = void (*)(const std::string& text, void* user);
    using MuteHotkeyCallback = void (*)(void* user);

    bool init(HWND parent);
    void destroy();

    void open();
    void close(bool  = false);
    bool isOpen() const { return open_; }

    void setInputAllowed(bool allowed);
    bool isInputAllowed() const { return inputAllowed_; }

    void setOnSend(SendCallback cb, void* user);
    void setOnMuteHotkey(MuteHotkeyCallback cb, void* user);

    void layout();

private:
    static LRESULT CALLBACK parentHook(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    static LRESULT CALLBACK panelProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    static LRESULT CALLBACK editHook(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    void sendAndClose();
    bool createPanel();

    HWND parent_ = nullptr;
    HWND panel_  = nullptr;
    HWND edit_   = nullptr;
    HFONT font_  = nullptr;
    WNDPROC oldParentProc_ = nullptr;
    WNDPROC oldEditProc_   = nullptr;
    bool open_ = false;
    bool inputAllowed_ = true;

    SendCallback onSend_ = nullptr;
    void* onSendUser_ = nullptr;
    MuteHotkeyCallback onMuteHotkey_ = nullptr;
    void* onMuteHotkeyUser_ = nullptr;
};
