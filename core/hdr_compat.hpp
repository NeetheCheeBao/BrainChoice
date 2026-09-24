#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef WINVER
#define WINVER 0x0A00
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#include <windows.h>

void hdrCompatEarlyInit();
bool hdrDisplayActive();
void hdrPrepareWindow(HWND hwnd);
void hdrPrepareDc(HDC hdc);
