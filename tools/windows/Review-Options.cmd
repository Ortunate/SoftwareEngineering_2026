@echo off
rem 启动副机独立审阅工具，参数转交 PowerShell 启动脚本。
setlocal
cd /d "%~dp0..\.."
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0..\..\tools\projectile_review\run.ps1" -Action options %*
if errorlevel 1 pause
endlocal
