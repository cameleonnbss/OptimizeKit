@echo off
rem ============================================================
rem  OptimizeKit - one-click build (Windows, MinGW-w64 g++)
rem  Output: dist\OptimizeKit.exe
rem ============================================================
setlocal enabledelayedexpansion
cd /d "%~dp0"

where g++ >nul 2>nul
if errorlevel 1 (
    if exist "C:\mingw64\bin" set "PATH=C:\mingw64\bin;%PATH%"
)
where g++ >nul 2>nul
if errorlevel 1 (
    echo [!] g++ not found. Install MinGW-w64 ^(e.g. winget install MartinStorsjo.LLVM-MinGW^) or MSYS2.
    exit /b 1
)

if not exist build mkdir build
if not exist dist  mkdir dist

echo [1/2] Compiling resources...
windres resources\OptimizeKit.rc -O coff -o build\OptimizeKit.res
if errorlevel 1 exit /b 1

echo [2/2] Compiling C++ ...
g++ -std=c++20 -O2 -municode -mwindows ^
    -Isrc -Isrc/core ^
    src\app\main.cpp src\app\cli.cpp ^
    src\core\common.cpp src\core\sysinfo.cpp src\core\tweaks.cpp src\core\cleaner.cpp ^
    src\core\engine.cpp src\core\ping.cpp src\core\drivers.cpp src\core\gameboost.cpp ^
    src\ui\ui.cpp ^
    build\OptimizeKit.res ^
    -o dist\OptimizeKit.exe ^
    -ld2d1 -ldwrite -lwindowscodecs -luser32 -lgdi32 -lshell32 -ladvapi32 -lole32 -loleaut32 ^
    -lshlwapi -liphlpapi -lws2_32 -lwinmm -luxtheme -ldwmapi -lpowrprof -lsetupapi -lpsapi -luuid ^
    -static -static-libgcc -static-libstdc++
if errorlevel 1 exit /b 1

echo.
echo [OK] Built dist\OptimizeKit.exe
echo      Run  dist\OptimizeKit.exe            (dashboard)
echo      Run  OptimizeKit-user.bat / OptimizeKit-admin.bat  (CLI)
endlocal
