# 本文件用项目内依赖构建道具查看器；Generate 生成资产，Capture 生成固定旋转帧与 GIF。
param([switch]$Generate,[switch]$Capture)
. "$PSScriptRoot/../projectile_review/environment.ps1"
$python="$ProjectRoot/.tools/blender-4.5.3-windows-x64/4.5/python/bin/python.exe"
function Check-Exit { if($LASTEXITCODE -ne 0){throw "道具工具失败：$LASTEXITCODE"} }
if($Generate){
 & $python -I tools/item_review/extract.py $ProjectRoot;Check-Exit
 & $BlenderExe --background --factory-startup --python-exit-code 1 --python tools/item_review/create.py -- $ProjectRoot;Check-Exit
}
& $CMakeExe -S tools/projectile_review -B build/projectile-review -G 'MinGW Makefiles' "-DCMAKE_MAKE_PROGRAM=$ProjectRoot/.tools/mingw/bin/mingw32-make.exe" "-DCMAKE_C_COMPILER=$ProjectRoot/.tools/mingw/bin/gcc.exe" "-DCMAKE_CXX_COMPILER=$ProjectRoot/.tools/mingw/bin/g++.exe" -DCMAKE_BUILD_TYPE=Release;Check-Exit
& $CMakeExe --build build/projectile-review --target item_inspector --parallel 4;Check-Exit
if($Capture){& "$ProjectRoot/build/projectile-review/item_inspector.exe" $ProjectRoot --capture;Check-Exit;& $python -I tools/item_review/encode.py $ProjectRoot;Check-Exit}
else{& "$ProjectRoot/build/projectile-review/item_inspector.exe" $ProjectRoot;Check-Exit}
