@echo off
rem 构建并运行正式游戏，参数转交 PowerShell 启动脚本。
setlocal
cd /d "%~dp0..\.."
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0..\..\tools\run_model_game.ps1" %*
if errorlevel 1 pause
endlocal
