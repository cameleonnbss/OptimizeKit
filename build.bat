@echo off
rem ============================================================
rem  OptimizeKit - one-click build (Windows, MinGW-w64 g++)
rem  Output: dist\OptimizeKit.exe (+ web\ assets next to it)
rem ============================================================
setlocal enabledelayedexpansion
cd /d "%~dp0"

where g++ >nul 2>nul
if errorlevel 1 (
    if exist "C:\mingw64\bin" set "PATH=C:\mingw64\bin;%PATH%"
)
where g++ >nul 2>nul
if errorlevel 1 (
    echo [!] g++ not found. Install MinGW-w64 ^(winlibs.com^) or MSYS2.
    exit /b 1
)

if not exist build mkdir build
if not exist dist  mkdir dist

rem --- embed the web dashboard into the exe (single-file guarantee) ---
rem the WindowsApps "python" can be a Store stub that hangs cmd; call it with an explicit flag
where python >nul 2>nul && (python -I tools\embed_web.py || exit /b 1) || echo [!] python missing - keeping the last generated webassets.h

echo [1/2] Compiling resources...
windres resources\OptimizeKit.rc -O coff -o build\OptimizeKit.res
if errorlevel 1 exit /b 1

echo [2/2] Compiling C++ ...
g++ -std=c++20 -O2 -municode -mwindows ^
    -Isrc -Isrc/core -DCPPHTTPLIB_THREAD_POOL_COUNT=4 ^
    src\app\main.cpp src\app\cli.cpp ^
    src\core\common.cpp src\core\sysinfo.cpp src\core\tweaks.cpp src\core\cleaner.cpp ^
    src\core\engine.cpp src\core\ping.cpp src\core\drivers.cpp src\core\gameboost.cpp ^
    src\core\monitor.cpp src\core\netprofile.cpp src\core\scan.cpp ^
    src\core\games.cpp src\core\ram.cpp src\core\storage.cpp src\core\logging2.cpp ^
    src\core\diagnostics.cpp ^
    src\core\reducer.cpp src\core\security.cpp src\core\diskscope.cpp ^
    src\core\firmware.cpp src\core\drvupdate.cpp ^
    src\server\server.cpp ^
    src\ui\ui.cpp src\ui\webframe.cpp ^
    build\OptimizeKit.res ^
    -o dist\OptimizeKit.exe ^
    -Ithird_party\webview2\include ^
    -ld2d1 -ldwrite -lwindowscodecs -luser32 -lgdi32 -lgdiplus -lshell32 -ladvapi32 -lole32 -loleaut32 ^
    -lshlwapi -liphlpapi -lws2_32 -lwinmm -luxtheme -ldwmapi -lpowrprof -lsetupapi -lpsapi ^
    -lpdh -lwininet -luuid -lntdll -lwintrust -lwbemuuid -lcfgmgr32 ^
    -static -static-libgcc -static-libstdc++
if errorlevel 1 exit /b 1

rem --- web assets next to the exe (embedded server serves them) ---
if not exist dist\web mkdir dist\web
copy /y web\index.html dist\web\ >nul
copy /y web\style.css  dist\web\ >nul
copy /y web\app.js     dist\web\ >nul
if not exist dist\web\assets mkdir dist\web\assets
if exist web\assets\fonts (
    if not exist dist\web\assets\fonts mkdir dist\web\assets\fonts
    copy /y web\assets\fonts\*.ttf dist\web\assets\fonts\ >nul
)
rem --- Game Library covers (Khadafi library, thumbnails only: not embedded in the exe) ---
if exist web\assets\gamelogos (
    if not exist dist\web\assets\gamelogos mkdir dist\web\assets\gamelogos
    copy /y web\assets\gamelogos\*.jpg dist\web\assets\gamelogos\ >nul
    copy /y web\assets\gamelogos\manifest.json dist\web\assets\gamelogos\ >nul
)
if not exist dist\web\fonts mkdir dist\web\fonts
if exist web\fonts copy /y web\fonts\*.woff2 dist\web\fonts\ >nul
if exist WebView2Loader.dll copy /y WebView2Loader.dll dist\ >nul

echo.
echo [OK] Built dist\OptimizeKit.exe + dist\web\
echo      Run  dist\OptimizeKit.exe   (web dashboard, standalone window)
echo      Run  OptimizeKit.bat (all-in-one) / OptimizeKit-cli.bat (menus)
endlocal
