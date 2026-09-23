@echo off
rem ============================================================
rem  OptimizeKit v2.8 - ALL-IN-ONE launcher (no exe required)
rem  The full PowerShell engine works with or without admin.
rem
rem  Just run:  OptimizeKit.bat
rem
rem  First launch offers UAC (full kit incl. all admin tweaks);
rem  declining falls back to the user menu automatically.
rem  Dashboard: [D] opens the app window (dist exe if built) and
rem  [W] runs the web dashboard without any exe.
rem  CLI without menus:
rem    OptimizeKit.bat -Status        read the machine
rem    OptimizeKit.bat -Apply id1,id2 apply specific tweaks
rem    OptimizeKit.bat -Profile gaming|privacy|debloat|full
rem    OptimizeKit.bat -Silent        gaming profile, no prompts
rem    OptimizeKit.bat -RestoreAll    back to Windows defaults
rem    OptimizeKit.bat -Firmware      BIOS / SecureBoot / TPM report
rem    OptimizeKit.bat -Drivers       driver age + vendor links
rem ============================================================
setlocal EnableExtensions
title OptimizeKit - Windows Gaming ^& Performance Control Center
cd /d "%~dp0"

set "ENGINE=PowerShell\OptimizeKit.ps1"
set "PSARGS=-NoProfile -ExecutionPolicy Bypass -File"

rem ---- pass CLI switches straight through: OptimizeKit.bat -Status ----
if not "%~1"=="" goto :passthru

echo.
echo   ==============================================
echo    OptimizeKit v2.8 - Control Center
echo   ==============================================
echo.
echo    --- DASHBOARD ---
echo    [D] App window  (dist\OptimizeKit.exe, if built)
echo    [W] Web dashboard in the browser (needs dist exe, loopback only)
echo.
echo    --- POWERSHELL ENGINE (no exe needed) ---
echo    [1] Full kit  (admin - all 71 tweaks, UAC prompt)
echo    [2] User kit  (no admin - HKCU-only tweaks)
echo    [3] Status    (read current state, no changes)
echo    [4] Individual tweaks (apply / restore, pick a number)
echo    [5] Network center (latency, DNS benchmark, fastest DNS)
echo    [6] Junk cleanup (temp, caches, recycle bin)
echo    [7] Firmware / BIOS (SecureBoot, TPM, VT - read only)
echo    [8] Drivers (GPU age, vendor page, Windows Update scan)
echo    [9] Restore all to Windows defaults (admin)
echo    [0] Exit
echo.
choice /C DW1234567890 /N /M "   Choose [D/W/1-9/0]: "
set "C=%errorlevel%"

if "%C%"=="1" ( if exist "dist\OptimizeKit.exe" ( start "" "dist\OptimizeKit.exe" & exit /b 0 ) else ( echo   [i] dist\OptimizeKit.exe not found - build with build.bat or use [W]. & pause & exit /b 0 ) )
if "%C%"=="2" (
    if exist "dist\OptimizeKit.exe" (
        start "" /min "dist\OptimizeKit.exe" --web 8765
        timeout /t 3 /nobreak >nul
        start "" http://127.0.0.1:8765
        echo   [i] Web dashboard served on http://127.0.0.1:8765 (loopback only).
        echo   [i] Close the minimized OptimizeKit tray task to stop the server.
        pause & exit /b 0
    ) else (
        echo   [i] The web dashboard needs dist\OptimizeKit.exe - build with build.bat.
        echo   [i] Without the exe, use the PowerShell engine menus below ^(1-9^).
        pause & exit /b 0
    )
)
if "%C%"=="3" ( powershell %PSARGS% "%ENGINE%" -Full        & pause & exit /b 0 )
if "%C%"=="4" ( powershell %PSARGS% "%ENGINE%" -User        & pause & exit /b 0 )
if "%C%"=="5" ( powershell %PSARGS% "%ENGINE%" -Status      & pause & exit /b 0 )
if "%C%"=="6" ( powershell %PSARGS% "%ENGINE%" -Tweaks      & pause & exit /b 0 )
if "%C%"=="7" ( powershell %PSARGS% "%ENGINE%" -Network     & pause & exit /b 0 )
if "%C%"=="8" ( powershell %PSARGS% "%ENGINE%" -Cleanup     & pause & exit /b 0 )
if "%C%"=="9" ( powershell %PSARGS% "%ENGINE%" -Firmware    & pause & exit /b 0 )
if "%C%"=="10" ( powershell %PSARGS% "%ENGINE%" -Drivers    & pause & exit /b 0 )
if "%C%"=="11" ( powershell %PSARGS% "%ENGINE%" -RestoreAll & pause & exit /b 0 )
if "%C%"=="12" ( exit /b 0 )
goto :eof

:passthru
powershell %PSARGS% "%ENGINE%" %*
pause
