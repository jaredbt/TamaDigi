@echo off
rem TamaDigi: local web installer (firmware, sprites, save file).
rem Just double-click it. Downloads the firmware from GitHub Pages and opens the
rem installer in Chrome/Edge at http://localhost:8765/web/
title TamaDigi Installer
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\local_installer.ps1" %*
if errorlevel 1 pause
