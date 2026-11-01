@echo off
rem 启动苦无独立审阅工具，参数转交 PowerShell 启动脚本。
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0..\..\tools\projectile_review\run.ps1" %*
set "EXIT_CODE=%ERRORLEVEL%"
if not "%EXIT_CODE%"=="0" pause
exit /b %EXIT_CODE%
