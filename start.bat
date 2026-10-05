@echo off
rem TamaDigi: lokaler Web-Installer (Firmware, Sprites, Spielstand).
rem Doppelklick genuegt. Laedt die Firmware von GitHub Pages und oeffnet den
rem Installer in Chrome/Edge unter http://localhost:8765/web/
title TamaDigi Installer
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\local_installer.ps1" %*
if errorlevel 1 pause
