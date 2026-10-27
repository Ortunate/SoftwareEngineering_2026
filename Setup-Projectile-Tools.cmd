@echo off
setlocal
rem Install portable tools inside this project only.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\projectile_review\setup.ps1"
set "EXIT_CODE=%ERRORLEVEL%"
if not "%EXIT_CODE%"=="0" pause
exit /b %EXIT_CODE%
