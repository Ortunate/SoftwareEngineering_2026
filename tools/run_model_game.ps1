# 本文件用项目内工具构建并运行正式三维模型版本；所有环境变量仅作用于当前子进程。
param([switch]$BuildOnly)
. "$PSScriptRoot/projectile_review/environment.ps1"
function Check-Exit { if($LASTEXITCODE -ne 0){throw "构建或运行失败，退出码 $LASTEXITCODE"} }
if(!(Test-Path -LiteralPath "$ProjectRoot/.deps/raylib-5.5/CMakeLists.txt")){throw '请先运行 Setup-Projectile-Tools.cmd 准备项目内依赖'}
& $CMakeExe -S $ProjectRoot -B "$ProjectRoot/build/model-game" -G 'MinGW Makefiles' "-DCMAKE_MAKE_PROGRAM=$ProjectRoot/.tools/mingw/bin/mingw32-make.exe" "-DCMAKE_C_COMPILER=$ProjectRoot/.tools/mingw/bin/gcc.exe" "-DCMAKE_CXX_COMPILER=$ProjectRoot/.tools/mingw/bin/g++.exe" "-DFETCHCONTENT_SOURCE_DIR_RAYLIB=$ProjectRoot/.deps/raylib-5.5" -DCMAKE_BUILD_TYPE=Release
Check-Exit
& $CMakeExe --build "$ProjectRoot/build/model-game" --target koumakyou --parallel 6
Check-Exit
if(!$BuildOnly){& "$ProjectRoot/build/model-game/koumakyou.exe";Check-Exit}
