#pragma once

struct LaunchSettings {
    int  clientW   = 405;
    int  clientH   = 720;
    int  bpp       = 32;
    int  refreshHz = 60;
    bool gpuMode   = true;
    bool vsync     = true;
};

bool runLauncher(LaunchSettings& out);
