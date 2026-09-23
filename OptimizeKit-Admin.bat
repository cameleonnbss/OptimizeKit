@echo off
rem ============================================================
rem  OptimizeKit v2.9 - CLI launcher, ADMIN VERSION
rem  Numbered menus through the PowerShell engine - no exe needed.
rem
rem  - double-clicked : offers UAC once. Accept -> full admin menu
rem    (all tweaks, profiles, cleanup, firmware, drivers, restore
rem    all). Decline -> continues without admin (user-safe set).
rem  - already elevated : no prompt, straight to the admin menu.
rem
rem  Optional: OptimizeKit-Admin.bat /exe uses the C++ CLI when
rem  dist\OptimizeKit.exe exists.
rem ============================================================
setlocal EnableExtensions
title OptimizeKit - CLI (admin)
cd /d "%~dp0"

set "ENGINE=PowerShell\OptimizeKit.ps1"
set "PSARGS=-NoProfile -ExecutionPolicy Bypass -File"

rem ---- /exe: prefer the compiled C++ CLI when it exists ----
if /i "%~1"=="/exe" (
    if exist "dist\OptimizeKit.exe" (
        "dist\OptimizeKit.exe" --cli
        echo.
        pause
        exit /b 0
    )
    echo [!] dist\OptimizeKit.exe not found - falling back to the PowerShell CLI.
)

net session >nul 2>&1
if "%errorlevel%"=="0" goto :elevated

rem ---- not elevated: offer UAC once ----
echo.
echo   OptimizeKit works best elevated: every tweak, cleanup, restore-all.
echo   [O] Reopen as administrator     [C] Continue without admin
choice /C OC /N /M "   Choose [O/C]: "
if "%errorlevel%"=="1" (
    powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b 0
)

:elevated
net session >nul 2>&1
set "MODE=admin"
if not "%errorlevel%"=="0" set "MODE=no-admin (user-safe tweaks only)"
echo.
echo   ==============================================
echo    OptimizeKit v2.9 - CLI menu [%MODE%]
echo   ==============================================
echo.
echo    [1] Full kit        - apply every tweak (reversible)
echo    [2] Status          - read the live state of every tweak
echo    [3] Individual      - apply / restore tweaks one by one
echo    [4] Profiles        - gaming / privacy / debloat / full
echo    [5] Network center  - latency, DNS benchmark, fastest DNS
echo    [6] Junk cleanup    - temp, caches, recycle bin
echo    [7] Firmware        - BIOS / SecureBoot / TPM (read-only)
echo    [8] Drivers         - GPU age, vendor page, WU driver scan
echo    [9] Restore ALL     - back to Windows defaults
echo    [0] Exit
echo.
choice /C 1234567890 /N /M "   Choose [1-9/0]: "
set "C=%errorlevel%"

if "%C%"=="1"  ( powershell %PSARGS% "%ENGINE%" -Full     & pause & exit /b 0 )
if "%C%"=="2"  ( powershell %PSARGS% "%ENGINE%" -Status   & pause & exit /b 0 )
if "%C%"=="3"  ( powershell %PSARGS% "%ENGINE%" -Tweaks   & pause & exit /b 0 )
if "%C%"=="4"  ( powershell %PSARGS% "%ENGINE%" -Profile  & pause & exit /b 0 )
if "%C%"=="5"  ( powershell %PSARGS% "%ENGINE%" -Network  & pause & exit /b 0 )
if "%C%"=="6"  ( powershell %PSARGS% "%ENGINE%" -Cleanup  & pause & exit /b 0 )
if "%C%"=="7"  ( powershell %PSARGS% "%ENGINE%" -Firmware & pause & exit /b 0 )
if "%C%"=="8"  ( powershell %PSARGS% "%ENGINE%" -Drivers  & pause & exit /b 0 )
if "%C%"=="9"  ( powershell %PSARGS% "%ENGINE%" -RestoreAll & pause & exit /b 0 )
if "%C%"=="10" ( exit /b 0 )
goto :eof
