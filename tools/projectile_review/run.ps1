# 本文件在子进程内构建、生成或运行独立弹体查看器，所有输出限定项目目录。
param([ValidateSet('view','collection','build','generate-classic','generate-collection','test','capture','capture-collection','stress','stress-quick','stress-collection','stress-collection-quick')][string]$Action='view')
. "$PSScriptRoot/environment.ps1"
function Check-Exit { if ($LASTEXITCODE -ne 0) { throw "工具失败，退出码 $LASTEXITCODE" } }
if ($Action -eq 'generate-classic') {
    & $BlenderExe --background --factory-startup --python-exit-code 1 --python tools/projectile_review/create_classic.py -- $ProjectRoot
    Check-Exit
    exit
}
if ($Action -eq 'generate-collection') {
    & $BlenderExe --background --factory-startup --python-exit-code 1 --python tools/projectile_review/create_collection.py -- $ProjectRoot
    Check-Exit
    exit
}
if ($Action -in @('build','test') -or !(Test-Path build/projectile-review/classic_inspector.exe) -or !(Test-Path build/projectile-review/collection_inspector.exe)) {
    & $CMakeExe -S tools/projectile_review -B build/projectile-review -G 'MinGW Makefiles' "-DCMAKE_MAKE_PROGRAM=$ProjectRoot/.tools/mingw/bin/mingw32-make.exe" "-DCMAKE_C_COMPILER=$ProjectRoot/.tools/mingw/bin/gcc.exe" "-DCMAKE_CXX_COMPILER=$ProjectRoot/.tools/mingw/bin/g++.exe" -DCMAKE_BUILD_TYPE=Release -DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF -DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF -DCMAKE_EXPORT_NO_PACKAGE_REGISTRY=ON
    Check-Exit
    & $CMakeExe --build build/projectile-review --parallel 6
    Check-Exit
}
if ($Action -eq 'test') {
    & "$ProjectRoot/.tools/blender-4.5.3-windows-x64/4.5/python/bin/python.exe" -I tools/projectile_review/verify_classic.py $ProjectRoot
    Check-Exit
    & "$ProjectRoot/.tools/blender-4.5.3-windows-x64/4.5/python/bin/python.exe" -I tools/projectile_review/verify_collection.py $ProjectRoot
    Check-Exit
    & "$ProjectRoot/.tools/cmake/bin/ctest.exe" --test-dir build/projectile-review --output-on-failure
    Check-Exit
    exit
}
if ($Action -eq 'build') { exit }
$argsForViewer = @($ProjectRoot)
$program = 'classic_inspector.exe'
if ($Action -in @('collection','capture-collection')) { $program = 'collection_inspector.exe' }
if ($Action -in @('capture','capture-collection')) { $argsForViewer += '--capture' }
if ($Action -in @('stress','stress-quick')) { $program = 'projectile_stress.exe' }
if ($Action -eq 'stress-quick') { $argsForViewer += '--quick' }
if ($Action -in @('stress-collection','stress-collection-quick')) { $program = 'collection_stress.exe' }
if ($Action -eq 'stress-collection-quick') { $argsForViewer += '--quick' }
& "$ProjectRoot/build/projectile-review/$program" @argsForViewer
Check-Exit
if ($Action -eq 'stress') {
    & "$ProjectRoot/.tools/blender-4.5.3-windows-x64/4.5/python/bin/python.exe" -I tools/projectile_review/summarize_performance.py $ProjectRoot
    Check-Exit
}
if ($Action -eq 'stress-collection') {
    & "$ProjectRoot/.tools/blender-4.5.3-windows-x64/4.5/python/bin/python.exe" -I tools/projectile_review/summarize_collection.py $ProjectRoot
    Check-Exit
}
