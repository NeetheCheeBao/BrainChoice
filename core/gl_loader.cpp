#include "core/gl_loader.hpp"
PFNGLCREATESHADERPROC            glCreateShader = nullptr;
PFNGLSHADERSOURCEPROC            glShaderSource = nullptr;
PFNGLCOMPILESHADERPROC           glCompileShader = nullptr;
PFNGLGETSHADERIVPROC             glGetShaderiv = nullptr;
PFNGLGETSHADERINFOLOGPROC        glGetShaderInfoLog = nullptr;
PFNGLDELETESHADERPROC            glDeleteShader = nullptr;
PFNGLCREATEPROGRAMPROC           glCreateProgram = nullptr;
PFNGLATTACHSHADERPROC            glAttachShader = nullptr;
PFNGLLINKPROGRAMPROC             glLinkProgram = nullptr;
PFNGLGETPROGRAMIVPROC            glGetProgramiv = nullptr;
PFNGLGETPROGRAMINFOLOGPROC       glGetProgramInfoLog = nullptr;
PFNGLUSEPROGRAMPROC              glUseProgram = nullptr;
PFNGLDELETEPROGRAMPROC           glDeleteProgram = nullptr;
PFNGLGETUNIFORMLOCATIONPROC      glGetUniformLocation = nullptr;
PFNGLUNIFORM1IPROC               glUniform1i = nullptr;
PFNGLUNIFORM1FPROC               glUniform1f = nullptr;
PFNGLUNIFORM2FPROC               glUniform2f = nullptr;
PFNGLUNIFORM3FPROC               glUniform3f = nullptr;
PFNGLUNIFORM4FPROC               glUniform4f = nullptr;
PFNGLUNIFORMMATRIX4FVPROC        glUniformMatrix4fv = nullptr;
PFNGLGENBUFFERSPROC              glGenBuffers = nullptr;
PFNGLBINDBUFFERPROC              glBindBuffer = nullptr;
PFNGLBUFFERDATAPROC              glBufferData = nullptr;
PFNGLDELETEBUFFERSPROC           glDeleteBuffers = nullptr;
PFNGLGENVERTEXARRAYSPROC         glGenVertexArrays = nullptr;
PFNGLBINDVERTEXARRAYPROC         glBindVertexArray = nullptr;
PFNGLDELETEVERTEXARRAYSPROC      glDeleteVertexArrays = nullptr;
PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray = nullptr;
PFNGLVERTEXATTRIBPOINTERPROC     glVertexAttribPointer = nullptr;
PFNGLACTIVETEXTUREPROC           glActiveTexture = nullptr;
PFNWGLSWAPINTERVALEXTPROC        wglSwapIntervalEXT = nullptr;
PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = nullptr;

static void* getProc(const char* name) {
    void* p = (void*)wglGetProcAddress(name);
    if (!p || p == (void*)0x1 || p == (void*)0x2 || p == (void*)0x3 || p == (void*)-1) {
        HMODULE mod = GetModuleHandleA("opengl32.dll");
        p = mod ? (void*)GetProcAddress(mod, name) : nullptr;
    }
    return p;
}

bool loadGLFunctions() {
#define LOAD(fn) do { \
    fn = (decltype(fn))getProc(#fn); \
    if (!fn) return false; \
} while (0)

    LOAD(glCreateShader);
    LOAD(glShaderSource);
    LOAD(glCompileShader);
    LOAD(glGetShaderiv);
    LOAD(glGetShaderInfoLog);
    LOAD(glDeleteShader);
    LOAD(glCreateProgram);
    LOAD(glAttachShader);
    LOAD(glLinkProgram);
    LOAD(glGetProgramiv);
    LOAD(glGetProgramInfoLog);
    LOAD(glUseProgram);
    LOAD(glDeleteProgram);
    LOAD(glGetUniformLocation);
    LOAD(glUniform1i);
    LOAD(glUniform1f);
    LOAD(glUniform2f);
    LOAD(glUniform3f);
    LOAD(glUniform4f);
    LOAD(glUniformMatrix4fv);
    LOAD(glGenBuffers);
    LOAD(glBindBuffer);
    LOAD(glBufferData);
    LOAD(glDeleteBuffers);
    LOAD(glGenVertexArrays);
    LOAD(glBindVertexArray);
    LOAD(glDeleteVertexArrays);
    LOAD(glEnableVertexAttribArray);
    LOAD(glVertexAttribPointer);
    LOAD(glActiveTexture);

#undef LOAD

    wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)getProc("wglSwapIntervalEXT");
    wglCreateContextAttribsARB =
        (PFNWGLCREATECONTEXTATTRIBSARBPROC)getProc("wglCreateContextAttribsARB");
    return true;
}
