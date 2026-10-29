@echo off
setlocal
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File tools/item_review/run.ps1 %*
if errorlevel 1 pause
endlocal
