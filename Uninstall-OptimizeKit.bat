@echo off
rem ============================================================
rem  OptimizeKit - uninstaller
rem  Removes the app data (logs, config, backups kept 1 copy).
rem  Tweaks stay applied on purpose - restore them with
rem  "OptimizeKit.exe --restore <id>" or the GUI Tweaks tab first.
rem ============================================================
title OptimizeKit - Uninstall
echo.
echo  This removes OptimizeKit app data only:
echo    %LOCALAPPDATA%\OptimizeKit
echo.
set /p CONF=Continue? (y/N):
if /i not "%CONF%"=="y" exit /b 0

if exist "%LOCALAPPDATA%\OptimizeKit" rmdir /s /q "%LOCALAPPDATA%\OptimizeKit"
echo [OK] OptimizeKit app data removed.
pause
