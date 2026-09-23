@echo off
rem ============================================================
rem  OptimizeKit v2.8 - CLI launcher (numbered menus, digits)
rem  Works WITHOUT the exe: everything runs through the
rem  PowerShell engine (PowerShell\OptimizeKit.ps1).
rem  - non elevated : user menu (HKCU tweaks, status, network...)
rem  - elevated     : full admin menu (71 tweaks, profiles,
rem                   cleanup, firmware, drivers, restore all)
rem  Optional: OptimizeKit-cli.bat /exe uses the C++ CLI when
rem  dist\OptimizeKit.exe exists.
rem ============================================================
setlocal EnableExtensions
title OptimizeKit - CLI
cd /d "%~dp0"

set "ENGINE=PowerShell\OptimizeKit.ps1"
set "PSARGS=-NoProfile -ExecutionPolicy Bypass -File"

if /i "%~1"=="/exe" (
    if exist "dist\OptimizeKit.exe" (
        "dist\OptimizeKit.exe" --cli
        echo.
        pause
        exit /b 0
    )
    echo [!] dist\OptimizeKit.exe not found - falling back to the PowerShell CLI.
)

powershell %PSARGS% "%ENGINE%"
echo.
pause
