@echo off
rem 启动灵梦弹体独立审阅工具，参数转交 PowerShell 启动脚本。
setlocal
cd /d "%~dp0..\.."
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0..\..\tools\projectile_review\run.ps1" -Action reimu %*
if errorlevel 1 pause
endlocal
