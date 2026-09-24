#include "core/hdr_compat.hpp"
#include <cstring>

static void mergePref(wchar_t* dst, size_t dstChars, const wchar_t* key, const wchar_t* want) {
    if (!dst || dstChars < 8 || !key || !want) return;
    wchar_t out[512];
    out[0] = 0;
    const wchar_t* p = dst;
    bool replaced = false;
    while (*p) {
        while (*p == L' ' || *p == L';') ++p;
        if (!*p) break;
        const wchar_t* start = p;
        while (*p && *p != L';') ++p;
        size_t n = (size_t)(p - start);
        wchar_t token[128];
        if (n >= 127) n = 127;
        for (size_t i = 0; i < n; ++i) token[i] = start[i];
        token[n] = 0;
        const size_t keyLen = lstrlenW(key);
        if (n >= keyLen && CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE,
                token, (int)keyLen, key, (int)keyLen) == CSTR_EQUAL && token[keyLen] == L'=') {
            if (lstrlenW(out) + lstrlenW(want) + 2 < 512) {
                lstrcatW(out, want);
                lstrcatW(out, L";");
            }
            replaced = true;
        } else if (n > 0) {
            if (lstrlenW(out) + (int)n + 2 < 512) {
                lstrcatW(out, token);
                lstrcatW(out, L";");
            }
        }
        if (*p == L';') ++p;
    }
    if (!replaced) {
        if (lstrlenW(out) + lstrlenW(want) + 2 < 512) {
            lstrcatW(out, want);
            lstrcatW(out, L";");
        }
    }
    lstrcpynW(dst, out, (int)dstChars);
}

static void writeGpuPreferences() {
    wchar_t exePath[MAX_PATH];
    if (!GetModuleFileNameW(nullptr, exePath, MAX_PATH) || !exePath[0])
        return;

    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\DirectX\\UserGpuPreferences",
            0, nullptr, 0, KEY_READ | KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS)
        return;

    wchar_t oldVal[512];
    oldVal[0] = 0;
    DWORD type = 0;
    DWORD cb = sizeof(oldVal);
    if (RegQueryValueExW(key, exePath, nullptr, &type, (LPBYTE)oldVal, &cb) != ERROR_SUCCESS ||
        type != REG_SZ) {
        oldVal[0] = 0;
    } else {
        oldVal[(sizeof(oldVal) / sizeof(oldVal[0])) - 1] = 0;
    }

    wchar_t val[512];
    lstrcpynW(val, oldVal, 512);
    mergePref(val, 512, L"AutoHDREnable", L"AutoHDREnable=0");
    mergePref(val, 512, L"SwapEffectUpgradeEnable", L"SwapEffectUpgradeEnable=0");
    mergePref(val, 512, L"GpuPreference", L"GpuPreference=2");

    const DWORD bytes = (DWORD)((lstrlenW(val) + 1) * sizeof(wchar_t));
    RegSetValueExW(key, exePath, 0, REG_SZ, (const BYTE*)val, bytes);
    RegCloseKey(key);
}

static void writeAppCompatFlags() {
    wchar_t exePath[MAX_PATH];
    if (!GetModuleFileNameW(nullptr, exePath, MAX_PATH) || !exePath[0])
        return;

    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Layers",
            0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS)
        return;

    const wchar_t* flags = L"~ DISABLEDXMAXIMIZEDWINDOWEDMODE HIGHDPIAWARE";
    RegSetValueExW(key, exePath, 0, REG_SZ,
                   (const BYTE*)flags, (DWORD)((lstrlenW(flags) + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
}

static void setDpiAwareness() {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (!user32) return;
    using Fn = BOOL (WINAPI*)(HANDLE);
    Fn fn = (Fn)GetProcAddress(user32, "SetProcessDpiAwarenessContext");
    if (fn)
        fn((HANDLE)(LONG_PTR)-4);
}

void hdrCompatEarlyInit() {
    SetEnvironmentVariableA("__GL_SYNC_TO_VBLANK", "0");
    setDpiAwareness();
    writeGpuPreferences();
    writeAppCompatFlags();
}

bool hdrDisplayActive() {
    UINT32 nPath = 0, nMode = 0;
    if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &nPath, &nMode) != ERROR_SUCCESS)
        return false;
    if (nPath == 0 || nPath > 64 || nMode > 256)
        return false;

    DISPLAYCONFIG_PATH_INFO paths[64];
    DISPLAYCONFIG_MODE_INFO modes[256];
    LONG q = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &nPath, paths, &nMode, modes, nullptr);
    if (q != ERROR_SUCCESS)
        return false;

    for (UINT32 i = 0; i < nPath; ++i) {
        struct {
            DISPLAYCONFIG_DEVICE_INFO_HEADER header;
            UINT32 value;
            UINT32 colorEncoding;
            UINT32 bitsPerColorChannel;
        } info;
        std::memset(&info, 0, sizeof(info));
        info.header.type = (DISPLAYCONFIG_DEVICE_INFO_TYPE)9;
        info.header.size = sizeof(info);
        info.header.adapterId = paths[i].targetInfo.adapterId;
        info.header.id = paths[i].targetInfo.id;
        if (DisplayConfigGetDeviceInfo(&info.header) != ERROR_SUCCESS)
            continue;
        if (info.value & 0x2u)
            return true;
    }
    return false;
}

void hdrPrepareWindow(HWND hwnd) {
    if (!hwnd) return;

    HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
    if (dwm) {
        using Fn = HRESULT (WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
        Fn setAttr = (Fn)GetProcAddress(dwm, "DwmSetWindowAttribute");
        if (setAttr) {
            const DWORD ncrpEnabled = 2;
            setAttr(hwnd, 2, &ncrpEnabled, sizeof(ncrpEnabled));
            const BOOL noTransition = TRUE;
            setAttr(hwnd, 3, &noTransition, sizeof(noTransition));
        }
        FreeLibrary(dwm);
    }
}

void hdrPrepareDc(HDC hdc) {
    if (!hdc) return;
#ifndef ICM_OFF
#define ICM_OFF 1
#endif
    SetICMMode(hdc, ICM_OFF);
}
