@echo off
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"

if not defined CXX set "CXX=g++"
if not defined WR  set "WR=windres"

where "%CXX%" >nul 2>&1
if errorlevel 1 (
  echo ERROR: g++ not found on PATH.
  echo Install MinGW-w64 and add its bin folder to PATH, or set CXX=...
  exit /b 1
)
where "%WR%" >nul 2>&1
if errorlevel 1 (
  echo ERROR: windres not found on PATH.
  echo Install MinGW-w64 and add its bin folder to PATH, or set WR=...
  exit /b 1
)

set "OUT=bin\BrainChoice.exe"
set "OBJDIR=%TEMP%\BrainChoice_build"
set "RES=%OBJDIR%\app_res.o"

set "SRCS=main.cpp app.cpp core\window.cpp core\gl_loader.cpp gfx\shader.cpp gfx\mesh.cpp gfx\texture.cpp scene\camera.cpp scene\scroll_controller.cpp scene\striped_scene.cpp scene\center_divider.cpp scene\character_sprite.cpp ui\launcher.cpp ui\chat_input.cpp ui\result_banner.cpp"

set /a TOTAL=2
for %%F in (%SRCS%) do set /a TOTAL+=1
set /a STEP=0

if not exist bin mkdir bin
if exist "%OBJDIR%" rmdir /s /q "%OBJDIR%" >nul 2>&1
mkdir "%OBJDIR%" 2>nul

taskkill /F /IM BrainChoice.exe >nul 2>&1

call :progress "resources (windres)"
"%WR%" -I . app.rc -o "%RES%"
if errorlevel 1 goto :fail

set "OBJS="
for %%F in (%SRCS%) do (
  set "name=%%F"
  set "name=!name:\=_!"
  set "name=!name:.cpp=.o!"
  call :progress "compile %%F"
  "%CXX%" -std=c++17 -O2 -Wall -I . -c "%%F" -o "%OBJDIR%\!name!"
  if errorlevel 1 goto :fail
  set OBJS=!OBJS! "%OBJDIR%\!name!"
)

call :progress "link %OUT%"
"%CXX%" !OBJS! "%RES%" -o "%OUT%" ^
  -mwindows ^
  -static-libgcc -static-libstdc++ -static ^
  -lopengl32 -lgdi32 -luser32 -lwinmm -lole32 -luuid -lwindowscodecs -limm32
if errorlevel 1 goto :fail

echo.
echo BUILD OK  -^> %OUT%
rmdir /s /q "%OBJDIR%" >nul 2>&1
call :hold 3
exit /b 0

:fail
echo.
echo BUILD FAILED
if exist "%OBJDIR%" rmdir /s /q "%OBJDIR%" >nul 2>&1
if exist bin\app_res.o del /f /q bin\app_res.o >nul 2>&1
echo.
pause
exit /b 1

:hold
echo.
timeout /t %~1 /nobreak >nul
goto :eof

:progress
set /a STEP+=1
set /a "pct=STEP*100/TOTAL"
set /a "filled=STEP*30/TOTAL"
if !filled! gtr 30 set "filled=30"
set "bar="
for /L %%i in (1,1,30) do (
  if %%i LEQ !filled! (set "bar=!bar!=") else (set "bar=!bar! ")
)
echo [!bar!] !pct!%%  %~1
goto :eof
