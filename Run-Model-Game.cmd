@echo off
setlocal
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_model_game.ps1 %*
if errorlevel 1 pause
endlocal
