#include "ui/launcher.hpp"
#include "core/window.hpp"
#include "resource.h"
#include <cstdio>
#include <cstring>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

enum {
    IDC_RES_COMBO   = 2003,
    IDC_ASP_COMBO   = 2005,
    IDC_GPU_CHECK   = 2006,
    IDC_VSYNC_CHECK = 2007,
    IDC_GO_BTN      = 2008,
    IDC_INFO_BTN    = 2009,
    IDC_QUIT_BTN    = 2010,
    IDC_COPY_OK     = 2100,
    IDC_WARN_OK     = 2101,
};

static constexpr int kClientW = 368;
static constexpr int kClientH = 232;
static constexpr int kPad     = 10;

struct ResEntry {
    int  w = 0, h = 0, bpp = 32, hz = 60;
    char label[64] = {};
};

struct LauncherState {
    HWND hwnd = nullptr;
    HWND resCombo = nullptr;
    HWND aspCombo = nullptr;
    HWND gpuCheck = nullptr;
    HWND vsyncCheck = nullptr;
    HFONT titleFont = nullptr;
    HFONT uiFont = nullptr;
    HBRUSH bgBrush = nullptr;
    std::vector<ResEntry> modes;
    LaunchSettings result{};
    bool gpuChecked = true;
    bool vsyncChecked = true;
    bool accepted = false;
    bool done = false;
};

static LauncherState* gLaunch = nullptr;

static COLORREF bgColor()   { return GetSysColor(COLOR_BTNFACE); }
static COLORREF textColor() { return GetSysColor(COLOR_BTNTEXT); }

static int clampPortrait(int w, int h, int& outW, int& outH) {
    outW = w;
    outH = h;
    enforcePortraitSize(outW, outH);
    return 1;
}

static int currentRefreshHz() {
    DEVMODEA dm{};
    dm.dmSize = sizeof(dm);
    if (EnumDisplaySettingsA(nullptr, ENUM_CURRENT_SETTINGS, &dm) &&
        dm.dmDisplayFrequency >= 1) {
        return (int)dm.dmDisplayFrequency;
    }
    return 60;
}

static void buildRefreshRates(std::vector<int>& rates) {
    rates.clear();
    const int fixed[] = { 24, 30, 60 };
    for (int hz : fixed) rates.push_back(hz);

    const int monitorHz = currentRefreshHz();
    bool already = false;
    for (int r : rates) {
        if (r == monitorHz) { already = true; break; }
    }
    if (!already && monitorHz > 0)
        rates.push_back(monitorHz);

    for (size_t i = 1; i < rates.size(); ++i) {
        int key = rates[i];
        size_t j = i;
        while (j > 0 && rates[j - 1] > key) {
            rates[j] = rates[j - 1];
            --j;
        }
        rates[j] = key;
    }
}

static void buildModeList(std::vector<ResEntry>& modes) {
    modes.clear();
    static const int kHeights[] = {
        720, 800, 960, 1080, 1280, 1440, 1600, 1920, 2160, 2560, 2880, 3840
    };

    std::vector<int> rates;
    buildRefreshRates(rates);

    for (int h : kHeights) {
        int w = (int)(h * kPortraitAspect + 0.5f);
        int cw = w, ch = h;
        clampPortrait(w, h, cw, ch);
        if (cw < kMinClientW || ch < kMinClientH) continue;
        if (cw > kMaxClientW || ch > kMaxClientH) continue;

        for (int hz : rates) {
            ResEntry e;
            e.w = cw;
            e.h = ch;
            e.bpp = 32;
            e.hz = hz;
            std::snprintf(e.label, sizeof(e.label),
                          "%dx%d 32bpp @%dhz", e.w, e.h, e.hz);
            modes.push_back(e);
        }
    }

    if (modes.empty()) {
        ResEntry e;
        e.w = kMinClientW;
        e.h = kMinClientH;
        e.bpp = 32;
        e.hz = currentRefreshHz();
        std::snprintf(e.label, sizeof(e.label), "%dx%d 32bpp @%dhz", e.w, e.h, e.hz);
        modes.push_back(e);
    }
}

static int pickDefaultModeIndex(const std::vector<ResEntry>& modes) {
    const int curHz = currentRefreshHz();
    for (size_t i = 0; i < modes.size(); ++i) {
        if (modes[i].w == kMinClientW && modes[i].h == kMinClientH && modes[i].hz == curHz)
            return (int)i;
    }
    for (size_t i = 0; i < modes.size(); ++i) {
        if (modes[i].w == kMinClientW && modes[i].h == kMinClientH)
            return (int)i;
    }
    return 0;
}

static void centerWindow(HWND hwnd) {
    RECT rc{};
    GetWindowRect(hwnd, &rc);
    const int ww = rc.right - rc.left;
    const int wh = rc.bottom - rc.top;
    const int sw = GetSystemMetrics(SM_CXSCREEN);
    const int sh = GetSystemMetrics(SM_CYSCREEN);
    SetWindowPos(hwnd, nullptr,
                 (sw - ww) / 2, (sh - wh) / 2,
                 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

static void applyUiFont(HWND parent, HFONT font) {
    for (HWND c = GetWindow(parent, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT)) {
        SendMessageA(c, WM_SETFONT, (WPARAM)font, TRUE);
    }
}

static void drawGroupFrame(HDC hdc, const RECT& r) {
    RECT t = r;
    DrawEdge(hdc, &t, EDGE_ETCHED, BF_RECT);
}

static void drawCenteredText(HDC hdc, const RECT& r, const char* text, HFONT font,
                             COLORREF color, bool underline = false) {
    HFONT old = font ? (HFONT)SelectObject(hdc, font) : nullptr;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    UINT flags = DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX;
    DrawTextA(hdc, text, -1, const_cast<RECT*>(&r), flags);
    if (underline) {
        SIZE sz{};
        GetTextExtentPoint32A(hdc, text, (int)strlen(text), &sz);
        const int cx = (r.left + r.right) / 2;
        const int y = (r.top + r.bottom) / 2 + sz.cy / 2 - 1;
        HPEN pen = CreatePen(PS_SOLID, 1, color);
        HGDIOBJ oldPen = SelectObject(hdc, pen);
        MoveToEx(hdc, cx - sz.cx / 2, y, nullptr);
        LineTo(hdc, cx + sz.cx / 2, y);
        SelectObject(hdc, oldPen);
        DeleteObject(pen);
    }
    if (old) SelectObject(hdc, old);
}

static void drawOwnerCheck(const DRAWITEMSTRUCT* dis, const char* text, bool checked, HFONT font) {
    HBRUSH bg = (gLaunch && gLaunch->bgBrush)
        ? gLaunch->bgBrush
        : GetSysColorBrush(COLOR_BTNFACE);
    FillRect(dis->hDC, &dis->rcItem, bg);

    RECT box = dis->rcItem;
    box.left += 2;
    box.top += (box.bottom - box.top - 13) / 2;
    box.right = box.left + 13;
    box.bottom = box.top + 13;
    UINT state = DFCS_BUTTONCHECK | DFCS_FLAT;
    if (checked) state |= DFCS_CHECKED;
    if (dis->itemState & ODS_DISABLED) state |= DFCS_INACTIVE;
    DrawFrameControl(dis->hDC, &box, DFC_BUTTON, state);

    RECT tr = dis->rcItem;
    tr.left = box.right + 6;
    HFONT old = font ? (HFONT)SelectObject(dis->hDC, font) : nullptr;
    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, textColor());
    DrawTextA(dis->hDC, text, -1, &tr,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    if (old) SelectObject(dis->hDC, old);

    if (dis->itemState & ODS_FOCUS) {
        RECT fr = dis->rcItem;
        InflateRect(&fr, -1, -1);
        DrawFocusRect(dis->hDC, &fr);
    }
}

static void paintLauncher(HWND hwnd, LauncherState* st) {
    PAINTSTRUCT ps{};
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT client{};
    GetClientRect(hwnd, &client);
    HBRUSH bg = (st && st->bgBrush) ? st->bgBrush : GetSysColorBrush(COLOR_BTNFACE);
    FillRect(hdc, &client, bg);

    RECT titleFrame = { kPad, 8, kClientW - kPad, 56 };
    drawGroupFrame(hdc, titleFrame);

    RECT titleText = { kPad + 8, 14, kClientW - kPad - 8, 50 };
    drawCenteredText(hdc, titleText, "Brain Choice",
                     st ? st->titleFont : nullptr,
                     textColor(), false);

    RECT optsFrame = { kPad, 64, kClientW - kPad, 164 };
    drawGroupFrame(hdc, optsFrame);

    HFONT ui = st ? st->uiFont : nullptr;
    const int colW = 148;
    const int col0 = kPad + 14;
    const int col1 = kPad + 190;
    RECT resLabel = { col0, 70, col0 + colW, 86 };
    RECT aspLabel = { col1, 70, col1 + colW, 86 };
    drawCenteredText(hdc, resLabel, "Resolution", ui, textColor(), false);
    drawCenteredText(hdc, aspLabel, "Monitor Aspect", ui, textColor(), false);

    EndPaint(hwnd, &ps);
}

struct CopyrightDlg {
    HWND hwnd = nullptr;
    bool done = false;
    HICON hWinIcon = nullptr;
    HICON hPortrait = nullptr;
    HFONT font = nullptr;
    HBRUSH bg = nullptr;
};

static CopyrightDlg* gCopy = nullptr;

static void centerOnParent(HWND hwnd, HWND parent) {
    RECT pr{}, wr{};
    GetWindowRect(parent ? parent : GetDesktopWindow(), &pr);
    GetWindowRect(hwnd, &wr);
    const int w = wr.right - wr.left;
    const int h = wr.bottom - wr.top;
    const int x = pr.left + ((pr.right - pr.left) - w) / 2;
    const int y = pr.top + ((pr.bottom - pr.top) - h) / 2;
    SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

static LRESULT CALLBACK CopyrightWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    CopyrightDlg* d = gCopy;
    switch (msg) {
    case WM_CREATE: {
        HINSTANCE inst = ((LPCREATESTRUCTA)lp)->hInstance;
        const int btnW = 80, btnH = 26;
        const int cw = 380, ch = 196;
        CreateWindowExA(0, "BUTTON", "OK",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
            (cw - btnW) / 2, ch - 36, btnW, btnH,
            hwnd, (HMENU)(INT_PTR)IDC_COPY_OK, inst, nullptr);
        if (d && d->font)
            SendMessageA(GetDlgItem(hwnd, IDC_COPY_OK), WM_SETFONT, (WPARAM)d->font, TRUE);
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT client{};
        GetClientRect(hwnd, &client);
        if (d && d->bg) FillRect(hdc, &client, d->bg);
        else FillRect(hdc, &client, GetSysColorBrush(COLOR_BTNFACE));

        const char* msgText = "Copyright by NeetheCheeBao";

        const wchar_t* tipEnter =
            L"\u2022 \"Enter\": Displays the message box / sends a message";
        const wchar_t* tipMute = L"\u2022 \"M\": Control audio";
        const wchar_t* tipEsc = L"\u2022 \"Esc\": Exit";
        const int iconS = 48;
        const int gap = 12;
        const int sidePad = 18;
        HFONT old = d && d->font ? (HFONT)SelectObject(hdc, d->font) : nullptr;
        SIZE tsz{};
        GetTextExtentPoint32A(hdc, msgText, (int)strlen(msgText), &tsz);
        const int blockW = iconS + gap + tsz.cx;
        const int blockH = iconS > tsz.cy ? iconS : tsz.cy;
        const int cx = (client.right - client.left) / 2;

        const int top = 14;
        const int left = cx - blockW / 2;
        const int iconY = top + (blockH - iconS) / 2;
        const int textY = top + (blockH - tsz.cy) / 2;

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, textColor());

        if (d && d->hPortrait) {
            DrawIconEx(hdc, left, iconY, d->hPortrait, iconS, iconS, 0, nullptr, DI_NORMAL);
        }
        RECT tr = { left + iconS + gap, textY, left + blockW, textY + tsz.cy };
        DrawTextA(hdc, msgText, -1, &tr,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        RECT tip0 = { sidePad, 78, client.right - sidePad, 98 };
        RECT tip1 = { sidePad, 98, client.right - sidePad, 118 };
        RECT tip2 = { sidePad, 118, client.right - sidePad, 138 };
        DrawTextW(hdc, tipEnter, -1, &tip0,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        DrawTextW(hdc, tipMute, -1, &tip1,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        DrawTextW(hdc, tipEsc, -1, &tip2,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        if (old) SelectObject(hdc, old);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == IDC_COPY_OK || LOWORD(wp) == IDOK) {
            if (d) d->done = true;
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_CLOSE:
        if (d) d->done = true;
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        if (d) d->hwnd = nullptr;
        return 0;
    default:
        break;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static void showCopyrightDialog(HWND parent) {
    CopyrightDlg dlg{};
    gCopy = &dlg;
    HINSTANCE inst = GetModuleHandleA(nullptr);

    dlg.hWinIcon = (HICON)LoadImageA(inst, MAKEINTRESOURCEA(IDI_INFO), IMAGE_ICON,
        GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0);
    if (!dlg.hWinIcon) {
        dlg.hWinIcon = (HICON)LoadImageA(nullptr, "assets\\ico\\info.ico", IMAGE_ICON,
            0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
    }
    dlg.hPortrait = (HICON)LoadImageA(inst, MAKEINTRESOURCEA(IDI_PORTRAIT), IMAGE_ICON,
        48, 48, 0);
    if (!dlg.hPortrait) {
        dlg.hPortrait = (HICON)LoadImageA(nullptr, "assets\\ico\\portrait.ico", IMAGE_ICON,
            48, 48, LR_LOADFROMFILE);
    }
    dlg.font = CreateFontA(
        14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "MS Shell Dlg 2");
    dlg.bg = CreateSolidBrush(bgColor());

    static bool sReg = false;
    if (!sReg) {
        WNDCLASSEXA wc = {};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = CopyrightWndProc;
        wc.hInstance = inst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hIcon = dlg.hWinIcon ? dlg.hWinIcon : LoadIcon(nullptr, IDI_APPLICATION);
        wc.hIconSm = dlg.hWinIcon;
        wc.lpszClassName = "BrainChoiceInfo";
        RegisterClassExA(&wc);
        sReg = true;
    }

    const int cw = 380, ch = 196;
    DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_BORDER;
    DWORD ex = WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE;
    RECT rc = { 0, 0, cw, ch };
    AdjustWindowRectEx(&rc, style, FALSE, ex);

    dlg.hwnd = CreateWindowExA(
        ex, "BrainChoiceInfo", "Info",
        style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        parent, nullptr, inst, nullptr);
    if (!dlg.hwnd) {
        gCopy = nullptr;
        if (dlg.font) DeleteObject(dlg.font);
        if (dlg.bg) DeleteObject(dlg.bg);
        return;
    }

    if (dlg.hWinIcon) {
        SendMessageA(dlg.hwnd, WM_SETICON, ICON_BIG, (LPARAM)dlg.hWinIcon);
        SendMessageA(dlg.hwnd, WM_SETICON, ICON_SMALL, (LPARAM)dlg.hWinIcon);
    }
    EnableWindow(parent, FALSE);
    centerOnParent(dlg.hwnd, parent);
    ShowWindow(dlg.hwnd, SW_SHOW);
    UpdateWindow(dlg.hwnd);

    MSG msg;
    while (!dlg.done && GetMessageA(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageA(dlg.hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }

    EnableWindow(parent, TRUE);
    SetForegroundWindow(parent);
    if (dlg.font) DeleteObject(dlg.font);
    if (dlg.bg) DeleteObject(dlg.bg);
    gCopy = nullptr;
}

struct WarnDlg {
    HWND hwnd = nullptr;
    bool done = false;
    HICON hWinIcon = nullptr;
    HFONT font = nullptr;
    HBRUSH bg = nullptr;
};

static WarnDlg* gWarn = nullptr;

static LRESULT CALLBACK WarnWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    WarnDlg* d = gWarn;
    switch (msg) {
    case WM_CREATE: {
        HINSTANCE inst = ((LPCREATESTRUCTA)lp)->hInstance;
        const int btnW = 80, btnH = 26;
        const int cw = 340, ch = 140;
        CreateWindowExA(0, "BUTTON", "OK",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
            (cw - btnW) / 2, ch - 36, btnW, btnH,
            hwnd, (HMENU)(INT_PTR)IDC_WARN_OK, inst, nullptr);
        if (d && d->font)
            SendMessageA(GetDlgItem(hwnd, IDC_WARN_OK), WM_SETFONT, (WPARAM)d->font, TRUE);
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT client{};
        GetClientRect(hwnd, &client);
        if (d && d->bg) FillRect(hdc, &client, d->bg);
        else FillRect(hdc, &client, GetSysColorBrush(COLOR_BTNFACE));

        const char* line0 = "Software mode is not available.";
        const char* line1 = "GPU Mode is required (OpenGL).";
        const int sidePad = 24;
        HFONT old = d && d->font ? (HFONT)SelectObject(hdc, d->font) : nullptr;
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, textColor());

        RECT r0 = { sidePad, 28, client.right - sidePad, 50 };
        RECT r1 = { sidePad, 50, client.right - sidePad, 72 };
        DrawTextA(hdc, line0, -1, &r0,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        DrawTextA(hdc, line1, -1, &r1,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        if (old) SelectObject(hdc, old);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == IDC_WARN_OK || LOWORD(wp) == IDOK) {
            if (d) d->done = true;
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_CLOSE:
        if (d) d->done = true;
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        if (d) d->hwnd = nullptr;
        return 0;
    default:
        break;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static void showWarnDialog(HWND parent) {
    WarnDlg dlg{};
    gWarn = &dlg;
    HINSTANCE inst = GetModuleHandleA(nullptr);

    dlg.hWinIcon = (HICON)LoadImageA(inst, MAKEINTRESOURCEA(IDI_WARN), IMAGE_ICON,
        GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0);
    if (!dlg.hWinIcon) {
        dlg.hWinIcon = (HICON)LoadImageA(nullptr, "assets\\ico\\warn.ico", IMAGE_ICON,
            0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
    }
    dlg.font = CreateFontA(
        14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "MS Shell Dlg 2");
    dlg.bg = CreateSolidBrush(bgColor());

    static bool sReg = false;
    if (!sReg) {
        WNDCLASSEXA wc = {};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WarnWndProc;
        wc.hInstance = inst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hIcon = dlg.hWinIcon ? dlg.hWinIcon : LoadIcon(nullptr, IDI_APPLICATION);
        wc.hIconSm = dlg.hWinIcon;
        wc.lpszClassName = "BrainChoiceWarn";
        RegisterClassExA(&wc);
        sReg = true;
    }

    const int cw = 340, ch = 140;
    DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_BORDER;
    DWORD ex = WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE;
    RECT rc = { 0, 0, cw, ch };
    AdjustWindowRectEx(&rc, style, FALSE, ex);

    dlg.hwnd = CreateWindowExA(
        ex, "BrainChoiceWarn", "Warn",
        style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        parent, nullptr, inst, nullptr);
    if (!dlg.hwnd) {
        gWarn = nullptr;
        if (dlg.font) DeleteObject(dlg.font);
        if (dlg.bg) DeleteObject(dlg.bg);
        return;
    }

    if (dlg.hWinIcon) {
        SendMessageA(dlg.hwnd, WM_SETICON, ICON_BIG, (LPARAM)dlg.hWinIcon);
        SendMessageA(dlg.hwnd, WM_SETICON, ICON_SMALL, (LPARAM)dlg.hWinIcon);
    }
    EnableWindow(parent, FALSE);
    centerOnParent(dlg.hwnd, parent);
    ShowWindow(dlg.hwnd, SW_SHOW);
    UpdateWindow(dlg.hwnd);

    MSG msg;
    while (!dlg.done && GetMessageA(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageA(dlg.hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }

    EnableWindow(parent, TRUE);
    SetForegroundWindow(parent);
    if (dlg.font) DeleteObject(dlg.font);
    if (dlg.bg) DeleteObject(dlg.bg);
    gWarn = nullptr;
}

static void onGo(LauncherState* st) {
    const int sel = (int)SendMessageA(st->resCombo, CB_GETCURSEL, 0, 0);
    if (sel >= 0 && sel < (int)st->modes.size()) {
        st->result.clientW   = st->modes[sel].w;
        st->result.clientH   = st->modes[sel].h;
        st->result.bpp       = st->modes[sel].bpp;
        st->result.refreshHz = st->modes[sel].hz;
    }
    st->result.gpuMode = st->gpuChecked;
    st->result.vsync   = st->vsyncChecked;

    if (!st->result.gpuMode) {
        showWarnDialog(st->hwnd);
        st->gpuChecked = true;
        InvalidateRect(st->gpuCheck, nullptr, TRUE);
        return;
    }

    st->accepted = true;
    st->done = true;
    DestroyWindow(st->hwnd);
}

static void onQuit(LauncherState* st) {
    st->accepted = false;
    st->done = true;
    DestroyWindow(st->hwnd);
}

static LRESULT CALLBACK LauncherWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    LauncherState* st = gLaunch;
    switch (msg) {
    case WM_CREATE: {
        HINSTANCE inst = ((LPCREATESTRUCTA)lp)->hInstance;

        const int colW = 148;
        const int col0 = kPad + 14;
        const int col1 = kPad + 190;
        const int comboY = 88;
        const int checkY = 122;
        const int btnY   = 178;
        const int btnW   = 72;
        const int btnH   = 24;
        const int btnGap = 10;
        const int btnTotal = btnW * 3 + btnGap * 2;
        const int btn0 = (kClientW - btnTotal) / 2;

        HWND res = CreateWindowExA(WS_EX_CLIENTEDGE, "COMBOBOX", "",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP,
            col0, comboY, colW, 160,
            hwnd, (HMENU)(INT_PTR)IDC_RES_COMBO, inst, nullptr);

        HWND asp = CreateWindowExA(WS_EX_CLIENTEDGE, "COMBOBOX", "",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_TABSTOP,
            col1, comboY, colW, 160,
            hwnd, (HMENU)(INT_PTR)IDC_ASP_COMBO, inst, nullptr);

        HWND gpu = CreateWindowExA(0, "BUTTON", "GPU Mode",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
            col0 + 8, checkY, 120, 22,
            hwnd, (HMENU)(INT_PTR)IDC_GPU_CHECK, inst, nullptr);

        HWND vsync = CreateWindowExA(0, "BUTTON", "V-sync",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
            col1 + 20, checkY, 100, 22,
            hwnd, (HMENU)(INT_PTR)IDC_VSYNC_CHECK, inst, nullptr);

        CreateWindowExA(0, "BUTTON", "Go",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
            btn0, btnY, btnW, btnH,
            hwnd, (HMENU)(INT_PTR)IDC_GO_BTN, inst, nullptr);

        CreateWindowExA(0, "BUTTON", "Info",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
            btn0 + btnW + btnGap, btnY, btnW, btnH,
            hwnd, (HMENU)(INT_PTR)IDC_INFO_BTN, inst, nullptr);

        CreateWindowExA(0, "BUTTON", "Quit",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
            btn0 + (btnW + btnGap) * 2, btnY, btnW, btnH,
            hwnd, (HMENU)(INT_PTR)IDC_QUIT_BTN, inst, nullptr);

        if (st) {
            st->hwnd = hwnd;
            st->resCombo = res;
            st->aspCombo = asp;
            st->gpuCheck = gpu;
            st->vsyncCheck = vsync;
            st->bgBrush = CreateSolidBrush(bgColor());

            st->titleFont = CreateFontA(
                28, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                DEFAULT_QUALITY, VARIABLE_PITCH | FF_ROMAN, "Times New Roman");
            st->uiFont = CreateFontA(
                13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "MS Shell Dlg 2");

            if (st->uiFont) applyUiFont(hwnd, st->uiFont);

            buildModeList(st->modes);
            for (const auto& m : st->modes)
                SendMessageA(res, CB_ADDSTRING, 0, (LPARAM)m.label);
            SendMessageA(res, CB_SETCURSEL, pickDefaultModeIndex(st->modes), 0);

            SendMessageA(asp, CB_ADDSTRING, 0, (LPARAM)"9:16 (fixed)");
            SendMessageA(asp, CB_SETCURSEL, 0, 0);
            EnableWindow(asp, FALSE);

            st->gpuChecked = true;
            st->vsyncChecked = true;
        }
        return 0;
    }

    case WM_PAINT:
        paintLauncher(hwnd, st);
        return 0;

    case WM_ERASEBKGND:

        return 1;

    case WM_DRAWITEM: {
        const auto* dis = reinterpret_cast<const DRAWITEMSTRUCT*>(lp);
        if (!st || !dis) return TRUE;
        if (dis->CtlID == IDC_GPU_CHECK) {
            drawOwnerCheck(dis, "GPU Mode", st->gpuChecked, st->uiFont);
            return TRUE;
        }
        if (dis->CtlID == IDC_VSYNC_CHECK) {
            drawOwnerCheck(dis, "V-sync", st->vsyncChecked, st->uiFont);
            return TRUE;
        }
        return TRUE;
    }

    case WM_COMMAND: {
        const int id = LOWORD(wp);
        if (id == IDC_GO_BTN) {
            if (st) onGo(st);
            return 0;
        }
        if (id == IDC_INFO_BTN) {
            showCopyrightDialog(hwnd);
            return 0;
        }
        if (id == IDC_QUIT_BTN || id == IDCANCEL) {
            if (st) onQuit(st);
            return 0;
        }
        if (id == IDC_GPU_CHECK && st) {
            st->gpuChecked = !st->gpuChecked;
            InvalidateRect(st->gpuCheck, nullptr, TRUE);
            return 0;
        }
        if (id == IDC_VSYNC_CHECK && st) {
            st->vsyncChecked = !st->vsyncChecked;
            InvalidateRect(st->vsyncCheck, nullptr, TRUE);
            return 0;
        }
        return 0;
    }

    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) {
            if (st) onQuit(st);
            return 0;
        }
        break;

    case WM_NCHITTEST: {
        LRESULT hit = DefWindowProcA(hwnd, msg, wp, lp);
        if (hit == HTCLIENT) {
            POINT pt = { (short)LOWORD(lp), (short)HIWORD(lp) };
            ScreenToClient(hwnd, &pt);
            HWND child = ChildWindowFromPointEx(hwnd, pt, CWP_SKIPINVISIBLE);
            if (!child || child == hwnd)
                return HTCAPTION;
        }
        return hit;
    }

    case WM_CLOSE:
        if (st) onQuit(st);
        return 0;

    case WM_CTLCOLORBTN: {
        HDC hdc = (HDC)wp;
        SetBkColor(hdc, bgColor());
        SetTextColor(hdc, textColor());
        return (LRESULT)((st && st->bgBrush) ? st->bgBrush : GetSysColorBrush(COLOR_BTNFACE));
    }

    case WM_DESTROY:
        if (st && st->hwnd == hwnd) st->hwnd = nullptr;
        if (st && !st->done) {
            st->accepted = false;
            st->done = true;
        }

        return 0;

    default:
        break;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

bool runLauncher(LaunchSettings& out) {
    LauncherState st;
    gLaunch = &st;

    HINSTANCE inst = GetModuleHandleA(nullptr);
    WNDCLASSEXA wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc   = LauncherWndProc;
    wc.hInstance     = inst;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = "BrainChoiceLauncher";

    HICON hIcon = (HICON)LoadImageA(inst, MAKEINTRESOURCEA(1), IMAGE_ICON,
        GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0);
    wc.hIcon   = hIcon ? hIcon : LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm = (HICON)LoadImageA(inst, MAKEINTRESOURCEA(1), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    if (!wc.hIconSm) wc.hIconSm = wc.hIcon;

    if (!RegisterClassExA(&wc)) {
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            gLaunch = nullptr;
            return false;
        }
    }

    const DWORD style = WS_POPUP | WS_BORDER | WS_CLIPCHILDREN;
    const DWORD exStyle = WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE;
    RECT rc = { 0, 0, kClientW, kClientH };
    AdjustWindowRectEx(&rc, style, FALSE, exStyle);
    const int winW = rc.right - rc.left;
    const int winH = rc.bottom - rc.top;

    HWND hwnd = CreateWindowExA(
        exStyle,
        "BrainChoiceLauncher",
        "Launcher",
        style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        winW, winH,
        nullptr, nullptr, inst, nullptr);

    if (!hwnd) {
        gLaunch = nullptr;
        return false;
    }

    centerWindow(hwnd);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (!st.done && GetMessageA(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageA(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }

    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT)
            continue;
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    if (st.titleFont) DeleteObject(st.titleFont);
    if (st.uiFont) DeleteObject(st.uiFont);
    if (st.bgBrush) DeleteObject(st.bgBrush);

    gLaunch = nullptr;

    if (!st.accepted) {
        return false;
    }

    out = st.result;
    return true;
}
