@echo off
rem 安装项目内隔离的弹体开发工具与依赖。
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0..\..\tools\projectile_review\setup.ps1"
set "EXIT_CODE=%ERRORLEVEL%"
if not "%EXIT_CODE%"=="0" pause
exit /b %EXIT_CODE%
