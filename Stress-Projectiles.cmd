@echo off
setlocal
rem Run the complete visible-window projectile collection benchmark.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\projectile_review\run.ps1" -Action stress-collection
set "EXIT_CODE=%ERRORLEVEL%"
if not "%EXIT_CODE%"=="0" pause
exit /b %EXIT_CODE%
