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
    src\server\server.cpp ^
    src\ui\ui.cpp ^
    build\OptimizeKit.res ^
    -o dist\OptimizeKit.exe ^
    -ld2d1 -ldwrite -lwindowscodecs -luser32 -lgdi32 -lgdiplus -lshell32 -ladvapi32 -lole32 -loleaut32 ^
    -lshlwapi -liphlpapi -lws2_32 -lwinmm -luxtheme -ldwmapi -lpowrprof -lsetupapi -lpsapi ^
    -lpdh -lwininet -luuid -lntdll ^
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
if not exist dist\web\fonts mkdir dist\web\fonts
if exist web\fonts copy /y web\fonts\*.woff2 dist\web\fonts\ >nul

echo.
echo [OK] Built dist\OptimizeKit.exe + dist\web\
echo      Run  dist\OptimizeKit.exe   (web dashboard, standalone window)
echo      Run  OptimizeKit-user.bat / OptimizeKit-admin.bat / OptimizeKit-cli.bat
endlocal
