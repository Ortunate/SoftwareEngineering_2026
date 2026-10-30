@echo off
setlocal
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File tools/projectile_review/run.ps1 -Action reimu %*
if errorlevel 1 pause
endlocal
