# Builds the portable ZIP and, when Inno Setup 6 or 7 is installed, the installer.
# The preset must be a Release build.
param(
    [string]$Preset = 'win-msvc-release',
    [string]$BuildDir = '',
    [string]$OutDir = 'build/dist',
    [switch]$SkipBuild
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
Set-Location $root

if (-not $SkipBuild) {
    cmake --preset $Preset
    if ($LASTEXITCODE) { throw 'configure failed' }
    cmake --build --preset $Preset
    if ($LASTEXITCODE) { throw 'build failed' }
}
if (-not $BuildDir) {
    $BuildDir = "build/$Preset"
}
$cache = Get-Content (Join-Path $BuildDir 'CMakeCache.txt')
$version = (($cache | Select-String '^CMAKE_PROJECT_VERSION:STATIC=(.*)$').Matches[0].Groups[1].Value)
$qtDir = (($cache | Select-String '^Qt6_DIR:PATH=(.*)$').Matches[0].Groups[1].Value)
$qtBin = Join-Path $qtDir '..\..\..\bin'

# Qt's translations are kept for the standard dialogs.
$stage = Join-Path $root 'build/stage'
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
cmake --install $BuildDir --prefix $stage --config Release
if ($LASTEXITCODE) { throw 'install failed' }
$deploy = Join-Path $qtBin 'windeployqt.exe'
& $deploy --no-system-d3d-compiler --svg --dir $stage `
    (Join-Path $stage 'ofs.exe') (Join-Path $stage 'ofscore.dll')
if ($LASTEXITCODE) { throw 'windeployqt failed' }
Get-ChildItem (Join-Path $stage 'engines') -Recurse -Filter *.dll | ForEach-Object {
    & $deploy --no-translations --no-system-d3d-compiler --dir $stage $_.FullName | Out-Null
}
# No Fighter Factory or engine file may ship: check against the commit hook's hash list.
$manifest = Join-Path $root '.git/ofs-forbidden.sha256'
if (Test-Path $manifest) {
    $forbidden = @{}
    Get-Content $manifest | ForEach-Object { if ($_ -match '^([0-9a-f]{64})') { $forbidden[$Matches[1]] = $true } }
    $hits = Get-ChildItem $stage -Recurse -File | Where-Object { $forbidden[(Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLower()] }
    if ($hits) { throw "forbidden files staged: $($hits.FullName -join ', ')" }
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$zip = Join-Path $OutDir "OpenFactoryStudio-$version-windows-portable.zip"
if (Test-Path $zip) { Remove-Item $zip }
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip
Write-Host "portable: $zip"

$iscc = (Get-Command ISCC.exe -ErrorAction SilentlyContinue).Source
if (-not $iscc) {
    # Inno Setup 6 or 7 (Chocolatey installs 7) in their default folders.
    foreach ($innoVersion in @('6', '7')) {
        foreach ($base in @(${env:ProgramFiles(x86)}, $env:ProgramFiles, "$env:LOCALAPPDATA\Programs")) {
            $candidate = Join-Path $base "Inno Setup $innoVersion\ISCC.exe"
            if ($base -and (Test-Path $candidate)) { $iscc = $candidate }
        }
    }
}
if ($iscc) {
    & $iscc "/DAppVersion=$version" "/DStageDir=$stage" "/DOutputDir=$((Resolve-Path $OutDir).Path)" `
        (Join-Path $PSScriptRoot 'ofs.iss')
    if ($LASTEXITCODE) { throw 'the installer failed to build' }
} else {
    Write-Warning 'Inno Setup not found: no installer built'
}
