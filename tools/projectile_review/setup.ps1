# 本文件按锁定清单安装便携工具与依赖，仅写项目根目录；不修改持久环境。
$ErrorActionPreference = 'Stop'
$ProjectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
Set-Location -LiteralPath $ProjectRoot
$lock = Get-Content -Raw "$PSScriptRoot/toolchain.lock.json" | ConvertFrom-Json
foreach ($folder in @('.tools','.deps','.cache/downloads','.cache/tmp')) {
    New-Item -ItemType Directory -Force (Join-Path $ProjectRoot $folder) | Out-Null
}
$env:TEMP = "$ProjectRoot/.cache/tmp"
$env:TMP = $env:TEMP
function Assert-Hash([string]$Path,[string]$Expected) {
    # 直接使用 .NET，避免从 PowerShell 7 启动 Windows PowerShell 时模块查找路径不同。
    $hashStream = [IO.File]::OpenRead($Path)
    $sha256 = [Security.Cryptography.SHA256]::Create()
    try { $actualHash = [BitConverter]::ToString($sha256.ComputeHash($hashStream)).Replace('-','').ToLowerInvariant() }
    finally { $hashStream.Dispose(); $sha256.Dispose() }
    if ($actualHash -ne $Expected) { throw "Checksum mismatch: $Path" }
}
foreach ($name in @('blender','cmake','mingw','raylib')) {
    $entry = $lock.$name
    $destination = Join-Path $ProjectRoot $entry.directory
    $marker = switch ($name) { blender {'blender.exe'} cmake {'bin/cmake.exe'} mingw {'bin/g++.exe'} raylib {'src/raylib.h'} }
    if (Test-Path -LiteralPath (Join-Path $destination $marker)) {
        if ($entry.binarySha256) { Assert-Hash (Join-Path $destination $marker) $entry.binarySha256 }
        Write-Host "Ready: $name $($entry.version)"
        continue
    }
    if (Test-Path -LiteralPath $destination) { throw "Incomplete directory exists; inspect before retrying: $destination" }
    $archive = Join-Path $ProjectRoot ".cache/downloads/$($entry.file)"
    if (!(Test-Path -LiteralPath $archive)) {
        $partial = "$archive.partial"
        & "$env:SystemRoot/System32/curl.exe" --fail --location --retry 2 --connect-timeout 20 --output $partial $entry.url
        if ($LASTEXITCODE -ne 0 -and $entry.mirror) {
            & "$env:SystemRoot/System32/curl.exe" --fail --location --retry 2 --connect-timeout 20 --output $partial $entry.mirror
        }
        if ($LASTEXITCODE -ne 0) { throw "Download failed: $name" }
        Assert-Hash $partial $entry.sha256
        Move-Item -LiteralPath $partial -Destination $archive
    }
    Assert-Hash $archive $entry.sha256
    $extractRoot = if ($name -eq 'raylib') { "$ProjectRoot/.deps" } else { "$ProjectRoot/.tools" }
    & "$env:SystemRoot/System32/tar.exe" -xf $archive -C $extractRoot
    if ($LASTEXITCODE -ne 0) { throw "Extraction failed: $name" }
    if ($name -in @('cmake','mingw')) {
        $extracted = if ($name -eq 'cmake') { "$ProjectRoot/.tools/cmake-3.31.5-windows-x86_64" } else { "$ProjectRoot/.tools/mingw64" }
        $sourceFull = [IO.Path]::GetFullPath($extracted)
        $destFull = [IO.Path]::GetFullPath($destination)
        $allowedRoot = [IO.Path]::GetFullPath("$ProjectRoot/.tools/")
        if (!$sourceFull.StartsWith($allowedRoot,[StringComparison]::OrdinalIgnoreCase) -or !$destFull.StartsWith($allowedRoot,[StringComparison]::OrdinalIgnoreCase)) { throw 'Extraction path escapes project tools' }
        Move-Item -LiteralPath $sourceFull -Destination $destFull
    }
    if (!(Test-Path -LiteralPath (Join-Path $destination $marker))) { throw "Missing tool after extraction: $name" }
    Write-Host "Installed: $name $($entry.version)"
}
Write-Host 'Project-local toolchain ready. No global installation or PATH changes.'
