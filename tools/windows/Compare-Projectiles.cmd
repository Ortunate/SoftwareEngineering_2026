@echo off
rem 本文件提供 Windows 启动入口，不改动玩家设置。
where py >nul 2>nul
if %errorlevel%==0 (
    py -3 "%~dp0..\compare_projectiles.py" %*
) else (
    python "%~dp0..\compare_projectiles.py" %*
)
exit /b %errorlevel%
