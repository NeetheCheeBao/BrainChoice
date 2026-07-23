#include "ui/chat_input.hpp"
#include <string>
#include <imm.h>
#include <shellapi.h>

static ChatInput* gChat = nullptr;

static constexpr int kMarginX   = 18;
static constexpr int kMarginBot = 28;
static constexpr int kBarH      = 46;
static constexpr int kPad       = 8;
static constexpr BYTE kAlpha    = 210;
static constexpr int kMaxInputChars = 8192;
static constexpr size_t kMaxClipboardScan = 65536;

bool ChatInput::createPanel() {
    HINSTANCE inst = (HINSTANCE)GetWindowLongPtrA(parent_, GWLP_HINSTANCE);
    if (!inst) inst = GetModuleHandleA(nullptr);

    static bool sClassOk = false;
    if (!sClassOk) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = panelProc;
        wc.hInstance = inst;
        wc.hCursor = LoadCursor(nullptr, IDC_IBEAM);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = L"BrainChoiceChatBar";
        ATOM atom = RegisterClassExW(&wc);
        if (!atom && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return false;
        }
        sClassOk = true;
    }

    panel_ = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW,
        L"BrainChoiceChatBar",
        L"",
        WS_POPUP | WS_CLIPCHILDREN,
        0, 0, 200, kBarH,
        parent_,
        nullptr,
        inst,
        this);
    if (!panel_) {
        panel_ = CreateWindowExW(
            WS_EX_TOOLWINDOW,
            L"BrainChoiceChatBar",
            L"",
            WS_POPUP | WS_CLIPCHILDREN,
            0, 0, 200, kBarH,
            parent_,
            nullptr,
            inst,
            this);
    }
    if (!panel_) return false;

    SetLayeredWindowAttributes(panel_, 0, kAlpha, LWA_ALPHA);

    edit_ = CreateWindowExW(
        0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_LEFT,
        kPad, kPad, 100, 28,
        panel_, nullptr, inst, nullptr);
    if (!edit_) {
        DestroyWindow(panel_);
        panel_ = nullptr;
        return false;
    }
    font_ = CreateFontW(
        -18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"MS Shell Dlg 2");
    if (font_) SendMessageW(edit_, WM_SETFONT, (WPARAM)font_, TRUE);

    SendMessageW(edit_, EM_LIMITTEXT, (WPARAM)kMaxInputChars, 0);
    DragAcceptFiles(edit_, FALSE);
    DragAcceptFiles(panel_, FALSE);

    oldEditProc_ = (WNDPROC)SetWindowLongPtrW(edit_, GWLP_WNDPROC, (LONG_PTR)editHook);
    ImmAssociateContext(edit_, nullptr);
    ShowWindow(panel_, SW_HIDE);
    return true;
}

bool ChatInput::init(HWND parent) {
    destroy();
    if (!parent) return false;
    parent_ = parent;
    gChat = this;
    oldParentProc_ = (WNDPROC)SetWindowLongPtrA(parent_, GWLP_WNDPROC, (LONG_PTR)parentHook);
    if (!createPanel()) return false;
    open_ = false;
    inputAllowed_ = true;
    layout();
    return true;
}

void ChatInput::destroy() {
    if (parent_ && oldParentProc_) {
        SetWindowLongPtrA(parent_, GWLP_WNDPROC, (LONG_PTR)oldParentProc_);
        oldParentProc_ = nullptr;
    }
    if (edit_ && oldEditProc_) {
        SetWindowLongPtrW(edit_, GWLP_WNDPROC, (LONG_PTR)oldEditProc_);
        oldEditProc_ = nullptr;
    }
    if (edit_) { DestroyWindow(edit_); edit_ = nullptr; }
    if (panel_) { DestroyWindow(panel_); panel_ = nullptr; }
    if (font_) { DeleteObject(font_); font_ = nullptr; }
    parent_ = nullptr;
    open_ = false;
    onSend_ = nullptr;
    onSendUser_ = nullptr;
    onMuteHotkey_ = nullptr;
    onMuteHotkeyUser_ = nullptr;
    if (gChat == this) gChat = nullptr;
}

void ChatInput::setInputAllowed(bool allowed) {
    inputAllowed_ = allowed;
    if (!allowed && open_) close();
}

void ChatInput::setOnSend(SendCallback cb, void* user) {
    onSend_ = cb;
    onSendUser_ = user;
}

void ChatInput::setOnMuteHotkey(MuteHotkeyCallback cb, void* user) {
    onMuteHotkey_ = cb;
    onMuteHotkeyUser_ = user;
}

void ChatInput::layout() {
    if (!parent_ || !panel_ || !edit_) return;
    RECT crc{};
    GetClientRect(parent_, &crc);
    const int cw = crc.right - crc.left;
    const int ch = crc.bottom - crc.top;
    if (cw < 40 || ch < 40) return;

    int barW = cw - kMarginX * 2;
    if (barW < 80) barW = 80;
    POINT bl{ kMarginX, ch - kMarginBot - kBarH };
    ClientToScreen(parent_, &bl);

    SetWindowPos(panel_, HWND_TOP, bl.x, bl.y, barW, kBarH,
                 SWP_NOACTIVATE | (open_ ? SWP_SHOWWINDOW : SWP_NOREDRAW));
    SetWindowPos(edit_, nullptr, kPad, kPad, barW - kPad * 2, kBarH - kPad * 2,
                 SWP_NOZORDER | SWP_NOACTIVATE);
}

void ChatInput::open() {
    if (!inputAllowed_ || !panel_ || !edit_) return;
    open_ = true;
    SetWindowTextW(edit_, L"");
    layout();
    ShowWindow(panel_, SW_SHOWNA);
    SetWindowPos(panel_, HWND_TOP, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    SetForegroundWindow(panel_);
    SetFocus(edit_);
    SendMessageW(edit_, EM_SETSEL, (WPARAM)-1, -1);
}

void ChatInput::close(bool) {
    if (!panel_) return;
    open_ = false;
    ShowWindow(panel_, SW_HIDE);
    if (parent_) {
        SetForegroundWindow(parent_);
        SetFocus(parent_);
    }
}

void ChatInput::sendAndClose() {
    if (!edit_) return;

    const int len = GetWindowTextLengthW(edit_);
    std::wstring wtext;
    if (len > 0) {
        wtext.resize((size_t)len + 1, L'\0');
        GetWindowTextW(edit_, wtext.data(), len + 1);
        wtext.resize((size_t)len);
    }

    std::string text;
    text.reserve(wtext.size() < (size_t)kMaxInputChars ? wtext.size() : (size_t)kMaxInputChars);
    for (wchar_t ch : wtext) {
        if (ch >= 0x20 && ch <= 0x7E) {
            text.push_back((char)ch);
            if ((int)text.size() >= kMaxInputChars)
                break;
        }
    }

    SetWindowTextW(edit_, L"");
    close();

    if (onSend_)
        onSend_(text, onSendUser_);
}

LRESULT CALLBACK ChatInput::parentHook(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    ChatInput* self = gChat;
    WNDPROC prev = self ? self->oldParentProc_ : DefWindowProcA;

    if (self) {
        if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) {
            if (wp == VK_RETURN) {
                if (!self->inputAllowed_) return 0;
                if (!self->open_) self->open();
                else self->sendAndClose();
                return 0;
            }
            if (wp == VK_ESCAPE && self->open_) {
                self->close();
                return 0;
            }

            if ((wp == 'M' || wp == 'm') && !self->open_) {
                if (self->onMuteHotkey_)
                    self->onMuteHotkey_(self->onMuteHotkeyUser_);
                return 0;
            }
        }
        if (msg == WM_SIZE || msg == WM_MOVE || msg == WM_EXITSIZEMOVE) {
            LRESULT r = CallWindowProcA(prev, hwnd, msg, wp, lp);
            if (self->open_) self->layout();
            return r;
        }
        if (msg == WM_ACTIVATE) {
            LRESULT r = CallWindowProcA(prev, hwnd, msg, wp, lp);
            if (self->open_) self->layout();
            return r;
        }
    }
    return CallWindowProcA(prev, hwnd, msg, wp, lp);
}

LRESULT CALLBACK ChatInput::panelProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        auto* cs = reinterpret_cast<LPCREATESTRUCTW>(lp);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return 0;
    }
    case WM_DROPFILES:
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc{};
        GetClientRect(hwnd, &rc);
        HBRUSH br = CreateSolidBrush(RGB(28, 28, 32));
        FillRect(hdc, &rc, br);
        DeleteObject(br);
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(110, 110, 120));
        HGDIOBJ oldPen = SelectObject(hdc, pen);
        HGDIOBJ oldBr = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBr);
        DeleteObject(pen);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wp;
        SetTextColor(hdc, RGB(240, 240, 245));
        SetBkColor(hdc, RGB(28, 28, 32));
        static HBRUSH sEditBr = CreateSolidBrush(RGB(28, 28, 32));
        return (LRESULT)sEditBr;
    }
    case WM_MOUSEACTIVATE:
        return MA_ACTIVATE;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static bool isAsciiPrintable(wchar_t ch) {
    return ch >= 0x20 && ch <= 0x7E;
}

static void pasteAsciiOnly(HWND edit) {
    if (!edit) return;

    if (!IsClipboardFormatAvailable(CF_UNICODETEXT))
        return;

    if (!OpenClipboard(edit)) return;

    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    if (!h) {
        CloseClipboard();
        return;
    }

    const SIZE_T rawBytes = GlobalSize(h);
    if (rawBytes < sizeof(wchar_t)) {
        CloseClipboard();
        return;
    }

    const wchar_t* p = (const wchar_t*)GlobalLock(h);
    if (!p) {
        CloseClipboard();
        return;
    }

    const int curLen = GetWindowTextLengthW(edit);
    int room = kMaxInputChars - curLen;
    if (room < 0) room = 0;

    std::wstring filtered;
    if (room > 0) {
        const size_t maxUnits = rawBytes / sizeof(wchar_t);
        size_t scanLimit = maxUnits;
        if (scanLimit > kMaxClipboardScan)
            scanLimit = kMaxClipboardScan;

        filtered.reserve((size_t)(room < 256 ? room : 256));
        for (size_t i = 0; i < scanLimit && (int)filtered.size() < room; ++i) {
            const wchar_t ch = p[i];
            if (ch == L'\0') break;
            if (isAsciiPrintable(ch))
                filtered.push_back(ch);
        }
    }

    GlobalUnlock(h);
    CloseClipboard();

    if (!filtered.empty())
        SendMessageW(edit, EM_REPLACESEL, TRUE, (LPARAM)filtered.c_str());
}

LRESULT CALLBACK ChatInput::editHook(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    ChatInput* self = gChat;
    WNDPROC prev = (self && self->oldEditProc_) ? self->oldEditProc_ : DefWindowProcW;

    if (msg == WM_DROPFILES)
        return 0;

    if (msg == WM_CHAR) {
        const wchar_t ch = (wchar_t)wp;
        if (ch == L'\r' || ch == L'\n' || ch == L'\t' || ch == 0x1B)
            return 0;
    }
    if (msg == WM_KEYDOWN && (wp == VK_RETURN || wp == VK_ESCAPE)) {
        if (self && self->open_) {
            if (wp == VK_RETURN) self->sendAndClose();
            else self->close();
        }
        return 0;
    }

    if (self && self->open_) {
        if (msg == WM_KEYDOWN) {
            const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            if ((wp == 'V' || wp == 'v') && ctrl) {
                pasteAsciiOnly(hwnd);
                return 0;
            }
            if (wp == VK_INSERT && shift) {
                pasteAsciiOnly(hwnd);
                return 0;
            }
        }
        if (msg == WM_IME_STARTCOMPOSITION ||
            msg == WM_IME_COMPOSITION ||
            msg == WM_IME_CHAR ||
            msg == WM_IME_ENDCOMPOSITION) {
            return 0;
        }
        if (msg == WM_CHAR) {
            const wchar_t ch = (wchar_t)wp;
            if (ch == L'\b')
                return CallWindowProcW(prev, hwnd, msg, wp, lp);
            if (!isAsciiPrintable(ch))
                return 0;
            return CallWindowProcW(prev, hwnd, msg, wp, lp);
        }
        if (msg == WM_PASTE) {
            pasteAsciiOnly(hwnd);
            return 0;
        }
        if (msg == WM_CLEAR || msg == WM_CUT || msg == WM_COPY) {
            return CallWindowProcW(prev, hwnd, msg, wp, lp);
        }
    }
    return CallWindowProcW(prev, hwnd, msg, wp, lp);
}
