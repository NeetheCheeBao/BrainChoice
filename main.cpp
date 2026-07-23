#include "app.hpp"
#include "ui/launcher.hpp"
#include <windows.h>

static LONG WINAPI onCrash(EXCEPTION_POINTERS* ep) {
    const DWORD code = ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionCode : 0;
    char buf[160];
    wsprintfA(buf, "Program crashed (0x%08lX).", (unsigned long)code);
    MessageBoxA(nullptr, buf, "BrainChoice", MB_ICONERROR);
    return EXCEPTION_EXECUTE_HANDLER;
}

int main() {
    SetUnhandledExceptionFilter(onCrash);

    LaunchSettings settings;
    if (!runLauncher(settings)) {
        return 0;
    }

    App app;
    if (!app.init(settings)) {
        return 1;
    }

    const int code = app.run();
    app.shutdown();
    return code;
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    return main();
}
