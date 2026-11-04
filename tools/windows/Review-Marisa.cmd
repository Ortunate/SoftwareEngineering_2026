@echo off
rem 启动魔理沙弹体独立审阅工具，参数转交 PowerShell 启动脚本。
setlocal
cd /d "%~dp0..\.."
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0..\..\tools\projectile_review\run.ps1" -Action marisa %*
if errorlevel 1 pause
endlocal
