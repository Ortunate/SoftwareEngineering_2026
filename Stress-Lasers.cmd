@echo off
setlocal
if "%~1"=="" (
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\projectile_review\run.ps1" -Action stress-lasers
) else (
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\projectile_review\run.ps1" %*
)
set "EXIT_CODE=%ERRORLEVEL%"
if not "%EXIT_CODE%"=="0" pause
exit /b %EXIT_CODE%
