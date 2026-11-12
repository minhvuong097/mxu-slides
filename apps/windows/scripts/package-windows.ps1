<#
.SYNOPSIS
  Build and package MxU Slides for Windows x64 with Qt 6.12.

.EXAMPLE
  $env:QT_ROOT = "C:\Qt\6.12.0\msvc2022_64"
  .\apps\windows\scripts\package-windows.ps1

.EXAMPLE
  .\apps\windows\scripts\package-windows.ps1 -QtRoot "C:\Qt\6.12.0\msvc2022_64" -Configuration Debug
#>
[CmdletBinding()]
param(
    [string]$QtRoot = $env:QT_ROOT,
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [string]$BuildDir,
    [switch]$SkipTests
)

$ErrorActionPreference = "Stop"
$AppRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if (-not $BuildDir) {
    $BuildDir = Join-Path $AppRoot "build\windows-$($Configuration.ToLower())"
}
if (-not $QtRoot) {
    throw "QtRoot is required. Set QT_ROOT to the Qt 6.12 MSVC 2022 x64 kit, for example C:\Qt\6.12.0\msvc2022_64."
}
$QtRoot = (Resolve-Path $QtRoot).Path
$DeployTool = Join-Path $QtRoot "bin\windeployqt.exe"
if (-not (Test-Path $DeployTool)) {
    throw "windeployqt.exe was not found under $QtRoot\bin."
}

Write-Host "Configuring MxU Slides ($Configuration)..." -ForegroundColor Cyan
cmake -S $AppRoot -B $BuildDir -G "Visual Studio 17 2022" -A x64 `
    -DCMAKE_PREFIX_PATH="$QtRoot" -DMXU_CORE_ONLY=OFF -DBUILD_TESTING=ON
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }

Write-Host "Building..." -ForegroundColor Cyan
cmake --build $BuildDir --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { throw "CMake build failed." }

if (-not $SkipTests) {
    Write-Host "Running tests..." -ForegroundColor Cyan
    ctest --test-dir $BuildDir -C $Configuration --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "Tests failed." }
}

$Exe = Join-Path $BuildDir "$Configuration\MxU Slides.exe"
if (-not (Test-Path $Exe)) {
    throw "Built executable was not found at $Exe."
}

$DistRoot = Join-Path $AppRoot "dist"
$Dist = Join-Path $DistRoot "MxU-Slides-Windows-x64"
if (Test-Path $Dist) { Remove-Item -Recurse -Force $Dist }
New-Item -ItemType Directory -Path $Dist | Out-Null

Write-Host "Collecting Qt runtime..." -ForegroundColor Cyan
$DeployArgs = @("--no-translations", "--no-system-d3d-compiler", "--no-opengl-sw",
                "--compiler-runtime", $Exe)
if ($Configuration -eq "Release") { $DeployArgs = @("--release") + $DeployArgs }
if ($Configuration -eq "Debug") { $DeployArgs = @("--debug") + $DeployArgs }
& $DeployTool @DeployArgs
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed." }

Copy-Item (Join-Path $AppRoot "README.md") (Join-Path $Dist "README.md")
Copy-Item (Join-Path $AppRoot "..\..\LICENSE.md") (Join-Path $Dist "LICENSE.md") -ErrorAction SilentlyContinue

$Archive = Join-Path $DistRoot "MxU-Slides-Windows-x64.zip"
if (Test-Path $Archive) { Remove-Item -Force $Archive }
Compress-Archive -Path (Join-Path $Dist "*") -DestinationPath $Archive -CompressionLevel Optimal
Write-Host "Packaged: $Archive" -ForegroundColor Green
