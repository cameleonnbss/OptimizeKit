@echo off
rem ============================================================
rem  OptimizeKit - CLI launcher (numbered menus, choice by digits)
rem  - non elevated : user menu
rem  - elevated     : full admin menu (profiles, boost, clean...)
rem ============================================================
title OptimizeKit - CLI
cd /d "%~dp0"

if not exist "dist\OptimizeKit.exe" (
    echo [!] dist\OptimizeKit.exe not found - run build.bat first.
    pause
    exit /b 1
)

"dist\OptimizeKit.exe" --cli

echo.
pause
