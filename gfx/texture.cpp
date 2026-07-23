#include "gfx/texture.hpp"
#include <vector>
#include <cstring>
#include <cstdio>
#include <objbase.h>
#include <wincodec.h>

static bool fileExistsA(const char* p) {
    const DWORD a = GetFileAttributesA(p);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static bool resolveAssetPath(const char* rel, char* out, size_t outSz) {
    if (!rel || !out || outSz < 4) return false;

    char exe[MAX_PATH] = {};
    if (GetModuleFileNameA(nullptr, exe, MAX_PATH)) {
        char* slash = exe;
        for (char* p = exe; *p; ++p) {
            if (*p == '\\' || *p == '/') slash = p;
        }
        *slash = '\0';

        snprintf(out, outSz, "%s\\%s", exe, rel);
        if (fileExistsA(out)) return true;

        char* slash2 = exe;
        for (char* p = exe; *p; ++p) {
            if (*p == '\\' || *p == '/') slash2 = p;
        }
        if (slash2 != exe) {
            *slash2 = '\0';
            snprintf(out, outSz, "%s\\%s", exe, rel);
            if (fileExistsA(out)) return true;
        }
    }

    if (fileExistsA(rel)) {
        strncpy(out, rel, outSz - 1);
        out[outSz - 1] = '\0';
        return true;
    }
    return false;
}

static bool uploadRgba(Texture& tex, const unsigned char* rgba, UINT w, UINT h,
                       const char* tag) {

    const size_t rowBytes = (size_t)w * 4;
    std::vector<unsigned char> flipped((size_t)h * rowBytes);
    for (UINT y = 0; y < h; ++y) {
        memcpy(flipped.data() + (size_t)(h - 1 - y) * rowBytes,
               rgba + (size_t)y * rowBytes,
               rowBytes);
    }

    if (tex.id) {
        glDeleteTextures(1, &tex.id);
        tex.id = 0;
    }
    glGenTextures(1, &tex.id);
    glBindTexture(GL_TEXTURE_2D, tex.id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)w, (GLsizei)h,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, flipped.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    tex.width  = (int)w;
    tex.height = (int)h;
    return true;
}

static bool decodeWicToTexture(Texture& tex, IStream* stream, const char* tag) {
    IWICImagingFactory* factory = nullptr;
    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory));
    if (FAILED(hr) || !factory) {
        return false;
    }

    IWICBitmapDecoder* decoder = nullptr;
    hr = factory->CreateDecoderFromStream(
        stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr) || !decoder) {
        factory->Release();
        return false;
    }

    IWICBitmapFrameDecode* frame = nullptr;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr) || !frame) {
        decoder->Release();
        factory->Release();
        return false;
    }

    IWICFormatConverter* conv = nullptr;
    hr = factory->CreateFormatConverter(&conv);
    if (FAILED(hr) || !conv) {
        frame->Release();
        decoder->Release();
        factory->Release();
        return false;
    }

    hr = conv->Initialize(frame, GUID_WICPixelFormat32bppRGBA,
                          WICBitmapDitherTypeNone, nullptr, 0.0,
                          WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        conv->Release();
        frame->Release();
        decoder->Release();
        factory->Release();
        return false;
    }

    UINT w = 0, h = 0;
    conv->GetSize(&w, &h);
    if (w == 0 || h == 0 || w > 16384 || h > 16384) {
        conv->Release();
        frame->Release();
        decoder->Release();
        factory->Release();
        return false;
    }

    const size_t rowBytes = (size_t)w * 4;
    std::vector<unsigned char> raw((size_t)h * rowBytes);
    hr = conv->CopyPixels(nullptr, (UINT)rowBytes, (UINT)raw.size(), raw.data());
    conv->Release();
    frame->Release();
    decoder->Release();
    factory->Release();

    if (FAILED(hr)) {
        return false;
    }
    return uploadRgba(tex, raw.data(), w, h, tag);
}

static bool withCom(bool (*fn)(void*), void* ctx) {
    const HRESULT coHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool coOk = (coHr == S_OK || coHr == S_FALSE || coHr == RPC_E_CHANGED_MODE);
    const bool ok = fn(ctx);
    if (coOk && coHr == S_OK) CoUninitialize();
    return ok;
}

bool Texture::loadFromMemory(const void* data, size_t size, const char* debugName) {
    destroy();
    if (!data || size == 0) return false;

    struct Ctx {
        Texture* self;
        const void* data;
        size_t size;
        const char* name;
        bool ok;
    } ctx{this, data, size, debugName, false};

    auto body = [](void* p) -> bool {
        auto* c = static_cast<Ctx*>(p);
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, c->size);
        if (!hMem) return false;
        void* locked = GlobalLock(hMem);
        if (!locked) {
            GlobalFree(hMem);
            return false;
        }
        memcpy(locked, c->data, c->size);
        GlobalUnlock(hMem);

        IStream* stream = nullptr;

        HRESULT hr = CreateStreamOnHGlobal(hMem, TRUE, &stream);
        if (FAILED(hr) || !stream) {
            GlobalFree(hMem);
            return false;
        }
        c->ok = decodeWicToTexture(*c->self, stream, c->name);
        stream->Release();
        return c->ok;
    };

    return withCom(body, &ctx);
}

bool Texture::loadFromResource(int resId) {
    destroy();
    HMODULE mod = GetModuleHandleA(nullptr);
    HRSRC hrs = FindResourceA(mod, MAKEINTRESOURCEA(resId), RT_RCDATA);
    if (!hrs) {
        return false;
    }
    HGLOBAL hg = LoadResource(mod, hrs);
    if (!hg) {
        return false;
    }
    const void* data = LockResource(hg);
    const DWORD sz = SizeofResource(mod, hrs);
    if (!data || sz == 0) {
        return false;
    }
    char tag[64];
    snprintf(tag, sizeof(tag), "res:%d", resId);
    return loadFromMemory(data, (size_t)sz, tag);
}

bool Texture::loadFromFile(const char* path) {
    destroy();

    char resolved[MAX_PATH] = {};
    if (!resolveAssetPath(path, resolved, sizeof(resolved))) {
        return false;
    }

    FILE* f = fopen(resolved, "rb");
    if (!f) {
        return false;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 64 * 1024 * 1024) {
        fclose(f);
        return false;
    }
    std::vector<unsigned char> buf((size_t)sz);
    if (fread(buf.data(), 1, (size_t)sz, f) != (size_t)sz) {
        fclose(f);
        return false;
    }
    fclose(f);
    return loadFromMemory(buf.data(), buf.size(), resolved);
}

void Texture::bind(int unit) const {
    if (!id) return;
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, id);
}

void Texture::destroy() {
    if (id) {
        glDeleteTextures(1, &id);
        id = 0;
    }
    width = height = 0;
}
