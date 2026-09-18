@echo off
rem ============================================================
rem  OptimizeKit - ADMIN launcher (self-elevates via UAC)
rem  Applies the full kit: all tweaks + cleanup + drivers pages.
rem ============================================================
title OptimizeKit - Admin
cd /d "%~dp0"

rem --- already elevated? ---
net session >nul 2>&1
if %errorlevel%==0 goto :run

echo Requesting administrator rights (confirm the UAC prompt)...
powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
exit /b 0

:run
if exist "dist\OptimizeKit.exe" (
    start "" "dist\OptimizeKit.exe"
    exit /b 0
)

echo [!] dist\OptimizeKit.exe not found - build it first: build.bat
echo.
echo Falling back to the PowerShell engine (full kit)...
powershell -NoProfile -ExecutionPolicy Bypass -File "PowerShell\OptimizeKit.ps1" -Full
pause
