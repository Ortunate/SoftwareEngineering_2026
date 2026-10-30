# 本文件只为当前子进程配置项目内路径；由启动器通过新 PowerShell 进程调用。
$ErrorActionPreference = 'Stop'
$ProjectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
Set-Location -LiteralPath $ProjectRoot
foreach ($relative in @('.cache/tmp','.cache/python','.local/blender/config','.local/blender/scripts','.local/blender/datafiles','.local/logs','output/projectile-review')) {
    New-Item -ItemType Directory -Force (Join-Path $ProjectRoot $relative) | Out-Null
}
$env:PATH = "$ProjectRoot/.tools/mingw/bin;$ProjectRoot/.tools/cmake/bin;$env:SystemRoot/System32;$env:SystemRoot"
$env:TEMP = "$ProjectRoot/.cache/tmp"
$env:TMP = $env:TEMP
$env:BLENDER_USER_RESOURCES = "$ProjectRoot/.local/blender"
$env:BLENDER_USER_CONFIG = "$ProjectRoot/.local/blender/config"
$env:BLENDER_USER_SCRIPTS = "$ProjectRoot/.local/blender/scripts"
$env:BLENDER_USER_DATAFILES = "$ProjectRoot/.local/blender/datafiles"
$env:PYTHONNOUSERSITE = '1'
$env:PYTHONPYCACHEPREFIX = "$ProjectRoot/.cache/python"
$env:PYTHONPATH = ''
$env:PIP_CACHE_DIR = "$ProjectRoot/.cache/pip"
$env:PIP_CONFIG_FILE = 'NUL'
$env:CMAKE_EXPORT_NO_PACKAGE_REGISTRY = 'ON'
$BlenderExe = "$ProjectRoot/.tools/blender-4.5.3-windows-x64/blender.exe"
$CMakeExe = "$ProjectRoot/.tools/cmake/bin/cmake.exe"
