@echo off
rem 启动道具独立审阅工具，参数转交 PowerShell 启动脚本。
setlocal
cd /d "%~dp0..\.."
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0..\..\tools\item_review\run.ps1" %*
if errorlevel 1 pause
endlocal
