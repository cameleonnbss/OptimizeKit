@echo off
rem ============================================================
rem  OptimizeKit - USER launcher (no admin rights needed)
rem  Opens the liquid-glass dashboard.
rem ============================================================
title OptimizeKit - User
cd /d "%~dp0"

if exist "dist\OptimizeKit.exe" (
    start "" "dist\OptimizeKit.exe"
    exit /b 0
)

echo [!] dist\OptimizeKit.exe not found - build it first:
echo     build.bat
echo.
echo Falling back to the PowerShell engine (user-safe tweaks)...
powershell -NoProfile -ExecutionPolicy Bypass -File "PowerShell\OptimizeKit.ps1" -User
pause
