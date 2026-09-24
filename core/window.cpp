#include "core/window.hpp"
#include "core/gl_loader.hpp"
#include "core/hdr_compat.hpp"
#include <cstdint>

#ifndef PFD_SUPPORT_COMPOSITION
#define PFD_SUPPORT_COMPOSITION 0x00008000
#endif
#ifndef PFD_SWAP_COPY
#define PFD_SWAP_COPY 0x00000400
#endif

#ifndef WGL_DRAW_TO_WINDOW_ARB
#define WGL_DRAW_TO_WINDOW_ARB            0x2001
#define WGL_ACCELERATION_ARB              0x2003
#define WGL_SWAP_METHOD_ARB               0x2007
#define WGL_SUPPORT_OPENGL_ARB            0x2010
#define WGL_DOUBLE_BUFFER_ARB             0x2011
#define WGL_PIXEL_TYPE_ARB                0x2013
#define WGL_RED_BITS_ARB                  0x2015
#define WGL_GREEN_BITS_ARB                0x2017
#define WGL_BLUE_BITS_ARB                 0x2019
#define WGL_ALPHA_BITS_ARB                0x201B
#define WGL_DEPTH_BITS_ARB                0x2022
#define WGL_STENCIL_BITS_ARB              0x2023
#define WGL_FULL_ACCELERATION_ARB         0x2027
#define WGL_SWAP_COPY_ARB                 0x2029
#define WGL_TYPE_RGBA_ARB                 0x202B
#define WGL_SAMPLE_BUFFERS_ARB            0x2041
#define WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB  0x20A9
#define WGL_TYPE_RGBA_FLOAT_ARB           0x21A0
#define WGL_COLORSPACE_EXT                0x309D
#define WGL_COLORSPACE_SRGB_EXT           0x3089
#endif

typedef BOOL (WINAPI *PFNWGLCHOOSEPIXELFORMATARBPROC)(HDC, const int*, const FLOAT*, UINT, int*, UINT*);
typedef BOOL (WINAPI *PFNWGLGETPIXELFORMATATTRIBIVARBPROC)(HDC, int, int, UINT, const int*, int*);

static AppWindow* gWin = nullptr;

static constexpr DWORD kWindowStyle =
    WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX;

static void* wglProc(const char* name) {
    void* p = (void*)wglGetProcAddress(name);
    if (!p || p == (void*)(uintptr_t)1 || p == (void*)(uintptr_t)2 ||
        p == (void*)(uintptr_t)3 || p == (void*)(uintptr_t)-1)
        return nullptr;
    return p;
}

void applyHdrSafePresent() {
    if (wglSwapIntervalEXT)
        wglSwapIntervalEXT(0);
}

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

static void onDisplayChange() {
    if (!gWin) return;
    gWin->displayChanged = true;
    if (gWin->hdc && gWin->hrc) {
        wglMakeCurrent(nullptr, nullptr);
        wglMakeCurrent(gWin->hdc, gWin->hrc);
        hdrPrepareDc(gWin->hdc);
    }
    applyHdrSafePresent();
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CLOSE:
        if (gWin) gWin->running = false;
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_DISPLAYCHANGE:
        onDisplayChange();
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

static void fillSdrPfd(PIXELFORMATDESCRIPTOR& pfd, bool composition, bool swapCopy) {
    pfd = {};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    if (composition) pfd.dwFlags |= PFD_SUPPORT_COMPOSITION;
    if (swapCopy) pfd.dwFlags |= PFD_SWAP_COPY;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24;
    pfd.cRedBits = 8;
    pfd.cGreenBits = 8;
    pfd.cBlueBits = 8;
    pfd.cAlphaBits = 8;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;
}

static bool bootstrapWglExts(PFNWGLCHOOSEPIXELFORMATARBPROC& choose,
                             PFNWGLCREATECONTEXTATTRIBSARBPROC& createCtx,
                             PFNWGLGETPIXELFORMATATTRIBIVARBPROC& getAttr) {
    choose = nullptr;
    createCtx = nullptr;
    getAttr = nullptr;

    HINSTANCE inst = GetModuleHandleA(nullptr);
    WNDCLASSA wc = {};
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = inst;
    wc.lpszClassName = "BrainChoiceGLDummy";
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(WS_EX_TOOLWINDOW, "BrainChoiceGLDummy", "",
                                WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, inst, nullptr);
    if (!hwnd) {
        UnregisterClassA("BrainChoiceGLDummy", inst);
        return false;
    }

    HDC hdc = GetDC(hwnd);
    PIXELFORMATDESCRIPTOR pfd;
    fillSdrPfd(pfd, true, true);
    int pf = ChoosePixelFormat(hdc, &pfd);
    if (!pf) {
        fillSdrPfd(pfd, true, false);
        pf = ChoosePixelFormat(hdc, &pfd);
    }
    if (!pf) {
        fillSdrPfd(pfd, false, false);
        pf = ChoosePixelFormat(hdc, &pfd);
    }

    bool ok = false;
    HGLRC rc = nullptr;
    if (pf && SetPixelFormat(hdc, pf, &pfd)) {
        rc = wglCreateContext(hdc);
        if (rc && wglMakeCurrent(hdc, rc)) {
            choose = (PFNWGLCHOOSEPIXELFORMATARBPROC)wglProc("wglChoosePixelFormatARB");
            createCtx = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglProc("wglCreateContextAttribsARB");
            getAttr = (PFNWGLGETPIXELFORMATATTRIBIVARBPROC)wglProc("wglGetPixelFormatAttribivARB");
            ok = choose != nullptr;
            wglMakeCurrent(nullptr, nullptr);
        }
    }
    if (rc) wglDeleteContext(rc);
    if (hdc) ReleaseDC(hwnd, hdc);
    DestroyWindow(hwnd);
    UnregisterClassA("BrainChoiceGLDummy", inst);
    return ok;
}

static bool formatIsSdr8(HDC hdc, int pf, PFNWGLGETPIXELFORMATATTRIBIVARBPROC getAttr) {
    if (!getAttr || pf <= 0) return true;
    const int keys[] = { WGL_RED_BITS_ARB, WGL_PIXEL_TYPE_ARB, WGL_ALPHA_BITS_ARB };
    int vals[3] = { 0, 0, 0 };
    if (!getAttr(hdc, pf, 0, 3, keys, vals))
        return true;
    if (vals[0] > 8) return false;
    if (vals[2] > 8) return false;
    if (vals[1] == (int)WGL_TYPE_RGBA_FLOAT_ARB) return false;
    return true;
}

static int chooseSdrFormatArb(HDC hdc,
                              PFNWGLCHOOSEPIXELFORMATARBPROC choose,
                              PFNWGLGETPIXELFORMATATTRIBIVARBPROC getAttr) {
    static const int kList0[] = {
        WGL_DRAW_TO_WINDOW_ARB, 1,
        WGL_SUPPORT_OPENGL_ARB, 1,
        WGL_DOUBLE_BUFFER_ARB, 1,
        WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
        WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
        WGL_RED_BITS_ARB, 8,
        WGL_GREEN_BITS_ARB, 8,
        WGL_BLUE_BITS_ARB, 8,
        WGL_ALPHA_BITS_ARB, 8,
        WGL_DEPTH_BITS_ARB, 24,
        WGL_STENCIL_BITS_ARB, 8,
        WGL_SAMPLE_BUFFERS_ARB, 0,
        WGL_SWAP_METHOD_ARB, WGL_SWAP_COPY_ARB,
        WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB, 1,
        WGL_COLORSPACE_EXT, WGL_COLORSPACE_SRGB_EXT,
        0
    };
    static const int kList1[] = {
        WGL_DRAW_TO_WINDOW_ARB, 1,
        WGL_SUPPORT_OPENGL_ARB, 1,
        WGL_DOUBLE_BUFFER_ARB, 1,
        WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
        WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
        WGL_RED_BITS_ARB, 8,
        WGL_GREEN_BITS_ARB, 8,
        WGL_BLUE_BITS_ARB, 8,
        WGL_ALPHA_BITS_ARB, 8,
        WGL_DEPTH_BITS_ARB, 24,
        WGL_STENCIL_BITS_ARB, 8,
        WGL_SAMPLE_BUFFERS_ARB, 0,
        WGL_SWAP_METHOD_ARB, WGL_SWAP_COPY_ARB,
        0
    };
    static const int kList2[] = {
        WGL_DRAW_TO_WINDOW_ARB, 1,
        WGL_SUPPORT_OPENGL_ARB, 1,
        WGL_DOUBLE_BUFFER_ARB, 1,
        WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
        WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
        WGL_RED_BITS_ARB, 8,
        WGL_GREEN_BITS_ARB, 8,
        WGL_BLUE_BITS_ARB, 8,
        WGL_ALPHA_BITS_ARB, 8,
        WGL_DEPTH_BITS_ARB, 24,
        WGL_STENCIL_BITS_ARB, 8,
        WGL_SAMPLE_BUFFERS_ARB, 0,
        0
    };
    static const int kList3[] = {
        WGL_DRAW_TO_WINDOW_ARB, 1,
        WGL_SUPPORT_OPENGL_ARB, 1,
        WGL_DOUBLE_BUFFER_ARB, 1,
        WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
        WGL_RED_BITS_ARB, 8,
        WGL_GREEN_BITS_ARB, 8,
        WGL_BLUE_BITS_ARB, 8,
        WGL_DEPTH_BITS_ARB, 24,
        0
    };
    const int* lists[] = { kList0, kList1, kList2, kList3 };

    for (const int* attribs : lists) {
        int fmts[32]{};
        UINT n = 0;
        if (!choose(hdc, attribs, nullptr, 32, fmts, &n) || n == 0)
            continue;
        for (UINT i = 0; i < n; ++i) {
            if (formatIsSdr8(hdc, fmts[i], getAttr))
                return fmts[i];
        }
    }
    return 0;
}

static bool setSdrPixelFormat(HDC hdc,
                              PFNWGLCHOOSEPIXELFORMATARBPROC choose,
                              PFNWGLGETPIXELFORMATATTRIBIVARBPROC getAttr) {
    PIXELFORMATDESCRIPTOR pfd;
    int pf = 0;
    if (choose)
        pf = chooseSdrFormatArb(hdc, choose, getAttr);
    if (pf > 0) {
        DescribePixelFormat(hdc, pf, sizeof(pfd), &pfd);
        if (SetPixelFormat(hdc, pf, &pfd))
            return true;
    }

    const bool tries[][2] = { {true, true}, {true, false}, {false, false} };
    for (const auto& t : tries) {
        fillSdrPfd(pfd, t[0], t[1]);
        pf = ChoosePixelFormat(hdc, &pfd);
        if (pf && SetPixelFormat(hdc, pf, &pfd))
            return true;
    }
    return false;
}

bool createAppWindow(AppWindow& w, const char* title, int clientW, int clientH) {
    gWin = &w;
    enforcePortraitSize(clientW, clientH);
    w.width = clientW;
    w.height = clientH;
    w.displayChanged = false;

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

    PFNWGLCHOOSEPIXELFORMATARBPROC choosePf = nullptr;
    PFNWGLCREATECONTEXTATTRIBSARBPROC createCtx = nullptr;
    PFNWGLGETPIXELFORMATATTRIBIVARBPROC getAttr = nullptr;
    bootstrapWglExts(choosePf, createCtx, getAttr);
    wglCreateContextAttribsARB = createCtx;

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

    hdrPrepareWindow(w.hwnd);

    if (hIconBig) SendMessageA(w.hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIconBig);
    if (hIconSm)  SendMessageA(w.hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSm);

    w.hdc = GetDC(w.hwnd);
    if (!w.hdc) {
        destroyAppWindow(w);
        return false;
    }
    hdrPrepareDc(w.hdc);

    if (!setSdrPixelFormat(w.hdc, choosePf, getAttr)) {
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

    if (!wglCreateContextAttribsARB)
        wglCreateContextAttribsARB =
            (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglProc("wglCreateContextAttribsARB");

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

    applyHdrSafePresent();

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
    glFlush();
    SwapBuffers(w.hdc);
}
