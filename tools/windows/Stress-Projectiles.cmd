@echo off
rem 启动普通弹体整组独立窗口压力测试。
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0..\..\tools\projectile_review\run.ps1" -Action stress-collection
set "EXIT_CODE=%ERRORLEVEL%"
if not "%EXIT_CODE%"=="0" pause
exit /b %EXIT_CODE%
