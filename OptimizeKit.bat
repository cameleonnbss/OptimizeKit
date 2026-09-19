@echo off
rem ============================================================
rem  OptimizeKit v2.1 - ALL-IN-ONE launcher
rem  The full PowerShell engine works with or without admin,
rem  and the dist exe is NOT required.
rem
rem  Just run:  OptimizeKit.bat
rem
rem  First launch offers UAC (full kit incl. 19 admin tweaks);
rem  declining falls back to the user menu automatically.
rem  CLI without menus:
rem    OptimizeKit.bat -Status        read the machine
rem    OptimizeKit.bat -Apply id1,id2 apply specific tweaks
rem    OptimizeKit.bat -Profile gaming|privacy|debloat|full
rem    OptimizeKit.bat -Silent        gaming profile, no prompts
rem    OptimizeKit.bat -RestoreAll    back to Windows defaults
rem ============================================================
setlocal
title OptimizeKit - Windows Gaming ^& Performance Control Center
cd /d "%~dp0"

set "ENGINE=PowerShell\OptimizeKit.ps1"
set "PSARGS=-NoProfile -ExecutionPolicy Bypass -File"

rem ---- pass CLI switches straight through: OptimizeKit.bat -Status ----
if not "%~1"=="" goto :passthru

echo.
echo   ==============================================
echo    OptimizeKit v2.1 - Control Center
echo   ==============================================
echo.
echo    [1] Full kit  (admin - all 49 tweaks, UAC prompt)
echo    [2] User kit  (no admin - HKCU-only tweaks)
echo    [3] Status    (read current state, no changes)
echo    [4] Restore all to Windows defaults (admin)
echo    [5] Dashboard exe (dist\OptimizeKit.exe, if built)
echo.
choice /C 12345 /N /M "   Choose [1-5]: "
set "C=%errorlevel%"

if "%C%"=="1" ( powershell %PSARGS% "%ENGINE%" -Full        & pause & exit /b 0 )
if "%C%"=="2" ( powershell %PSARGS% "%ENGINE%" -User        & pause & exit /b 0 )
if "%C%"=="3" ( powershell %PSARGS% "%ENGINE%" -Status      & pause & exit /b 0 )
if "%C%"=="4" ( powershell %PSARGS% "%ENGINE%" -RestoreAll  & pause & exit /b 0 )
if "%C%"=="5" (
    if exist "dist\OptimizeKit.exe" ( start "" "dist\OptimizeKit.exe" & exit /b 0 )
    echo   [i] dist\OptimizeKit.exe not found - use options 1-4 instead.
    pause & exit /b 0
)

:passthru
powershell %PSARGS% "%ENGINE%" %*
pause
