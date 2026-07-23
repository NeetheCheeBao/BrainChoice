#include "core/window.hpp"
#include "core/gl_loader.hpp"
static AppWindow* gWin = nullptr;

static constexpr DWORD kWindowStyle =
    WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX;

void enforcePortraitSize(int& w, int& h) {
    if (w < 1) w = 1;
    if (h < 1) h = 1;

    const int wFromH = (int)(h * kPortraitAspect + 0.5f);
    const int hFromW = (int)(w / kPortraitAspect + 0.5f);
    if (wFromH <= w) {
        w = wFromH > 0 ? wFromH : 1;
    } else {
        h = hFromW > 0 ? hFromW : 1;
        w = (int)(h * kPortraitAspect + 0.5f);
    }
    w = (int)(h * kPortraitAspect + 0.5f);
    if (w < 1) w = 1;
    if (h < 1) h = 1;

    if (h < kMinClientH) {
        h = kMinClientH;
        w = kMinClientW;
    }
    if (h > kMaxClientH) {
        h = kMaxClientH;
        w = kMaxClientW;
    }
    if (w < kMinClientW) {
        w = kMinClientW;
        h = kMinClientH;
    }
    if (w > kMaxClientW) {
        w = kMaxClientW;
        h = kMaxClientH;
    }
}

static void clientToWindowSize(DWORD style, int clientW, int clientH, int& outW, int& outH) {
    RECT rc = { 0, 0, clientW, clientH };
    AdjustWindowRect(&rc, style, FALSE);
    outW = rc.right - rc.left;
    outH = rc.bottom - rc.top;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CLOSE:
        if (gWin) gWin->running = false;
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_GETMINMAXINFO: {
        auto* mmi = reinterpret_cast<MINMAXINFO*>(lp);
        DWORD style = (DWORD)GetWindowLongA(hwnd, GWL_STYLE);
        int minW = 0, minH = 0, maxW = 0, maxH = 0;
        clientToWindowSize(style, kMinClientW, kMinClientH, minW, minH);
        clientToWindowSize(style, kMaxClientW, kMaxClientH, maxW, maxH);
        mmi->ptMinTrackSize.x = minW;
        mmi->ptMinTrackSize.y = minH;
        mmi->ptMaxTrackSize.x = maxW;
        mmi->ptMaxTrackSize.y = maxH;

        mmi->ptMaxSize.x = maxW;
        mmi->ptMaxSize.y = maxH;
        return 0;
    }

    case WM_SIZING: {

        RECT* r = reinterpret_cast<RECT*>(lp);
        DWORD style = (DWORD)GetWindowLongA(hwnd, GWL_STYLE);
        RECT adj = { 0, 0, 0, 0 };
        AdjustWindowRect(&adj, style, FALSE);
        const int frameW = adj.right - adj.left;
        const int frameH = adj.bottom - adj.top;

        int clientW = (r->right - r->left) - frameW;
        int clientH = (r->bottom - r->top) - frameH;
        if (clientW < 1) clientW = 1;
        if (clientH < 1) clientH = 1;

        switch (wp) {
        case WMSZ_LEFT:
        case WMSZ_RIGHT:
            clientH = (int)(clientW / kPortraitAspect + 0.5f);
            break;
        case WMSZ_TOP:
        case WMSZ_BOTTOM:
            clientW = (int)(clientH * kPortraitAspect + 0.5f);
            break;
        default:
            clientW = (int)(clientH * kPortraitAspect + 0.5f);
            break;
        }
        enforcePortraitSize(clientW, clientH);

        int winW = 0, winH = 0;
        clientToWindowSize(style, clientW, clientH, winW, winH);

        switch (wp) {
        case WMSZ_LEFT:
        case WMSZ_TOPLEFT:
        case WMSZ_BOTTOMLEFT:
            r->left = r->right - winW;
            break;
        default:
            r->right = r->left + winW;
            break;
        }
        switch (wp) {
        case WMSZ_TOP:
        case WMSZ_TOPLEFT:
        case WMSZ_TOPRIGHT:
            r->top = r->bottom - winH;
            break;
        default:
            r->bottom = r->top + winH;
            break;
        }
        return TRUE;
    }

    case WM_SIZE:
        if (gWin) {
            int cw = LOWORD(lp) > 1 ? (int)LOWORD(lp) : 1;
            int ch = HIWORD(lp) > 1 ? (int)HIWORD(lp) : 1;
            enforcePortraitSize(cw, ch);
            gWin->width = cw;
            gWin->height = ch;
        }
        return 0;

    case WM_SYSCOMMAND:

        if ((wp & 0xFFF0) == SC_MAXIMIZE) {
            return 0;
        }

        if ((wp & 0xFFF0) == SC_KEYMENU) {
            return 0;
        }
        break;

    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) {
            if (gWin) gWin->running = false;
        }
        return 0;
    default:
        break;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

bool createAppWindow(AppWindow& w, const char* title, int clientW, int clientH) {
    gWin = &w;
    enforcePortraitSize(clientW, clientH);
    w.width = clientW;
    w.height = clientH;

    HINSTANCE inst = GetModuleHandleA(nullptr);

    HICON hIconBig = (HICON)LoadImageA(inst, MAKEINTRESOURCEA(1), IMAGE_ICON,
        GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0);
    HICON hIconSm = (HICON)LoadImageA(inst, MAKEINTRESOURCEA(1), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);

    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hIcon = hIconBig ? hIconBig : LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm = hIconSm ? hIconSm : wc.hIcon;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = "BrainChoiceWnd";

    if (!RegisterClassExA(&wc)) {
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return false;
        }
    }

    DWORD style = kWindowStyle | WS_VISIBLE;

    int winW = 0, winH = 0;
    clientToWindowSize(style, clientW, clientH, winW, winH);

    w.hwnd = CreateWindowExA(
        0, "BrainChoiceWnd", title, style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        winW, winH,
        nullptr, nullptr, inst, nullptr);
    if (!w.hwnd) {
        gWin = nullptr;
        return false;
    }

    if (hIconBig) SendMessageA(w.hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIconBig);
    if (hIconSm)  SendMessageA(w.hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSm);

    w.hdc = GetDC(w.hwnd);
    if (!w.hdc) {
        destroyAppWindow(w);
        return false;
    }

    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int pf = ChoosePixelFormat(w.hdc, &pfd);
    if (!pf || !SetPixelFormat(w.hdc, pf, &pfd)) {
        destroyAppWindow(w);
        return false;
    }

    HGLRC tmp = wglCreateContext(w.hdc);
    if (!tmp) {
        destroyAppWindow(w);
        return false;
    }
    if (!wglMakeCurrent(w.hdc, tmp)) {
        wglDeleteContext(tmp);
        destroyAppWindow(w);
        return false;
    }

    wglCreateContextAttribsARB =
        (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");

    if (wglCreateContextAttribsARB) {
        const int attribs[] = {
            0x2091, 3,
            0x2092, 3,
            0x9126, 1,
            0
        };
        w.hrc = wglCreateContextAttribsARB(w.hdc, nullptr, attribs);
        if (w.hrc) {
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(tmp);
            if (!wglMakeCurrent(w.hdc, w.hrc)) {
                destroyAppWindow(w);
                return false;
            }
        } else {
            w.hrc = tmp;
        }
    } else {
        w.hrc = tmp;
    }

    if (!loadGLFunctions()) {
        destroyAppWindow(w);
        return false;
    }

    ShowWindow(w.hwnd, SW_SHOW);
    UpdateWindow(w.hwnd);
    return true;
}

void destroyAppWindow(AppWindow& w) {
    if (w.hrc) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(w.hrc);
        w.hrc = nullptr;
    }
    if (w.hdc && w.hwnd) {
        ReleaseDC(w.hwnd, w.hdc);
        w.hdc = nullptr;
    }
    if (w.hwnd) {
        DestroyWindow(w.hwnd);
        w.hwnd = nullptr;
    }
    gWin = nullptr;
}

void pollAppWindow(AppWindow& w) {
    MSG msg;
    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            w.running = false;
        }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

void swapAppWindow(AppWindow& w) {
    SwapBuffers(w.hdc);
}
