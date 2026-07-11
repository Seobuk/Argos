<#
.SYNOPSIS
  Reproducible build for Argos (Mayo fork): GUI app, argos-cli and unit tests.

.DESCRIPTION
  Configures and builds with the vcpkg toolchain (OpenCASCADE pinned to 7.9.0 via
  the repo vcpkg.json manifest) and a prebuilt Qt 6. Adjust the paths below or
  pass them as parameters if your environment differs.

.EXAMPLE
  pwsh -File scripts/argos-build.ps1
  pwsh -File scripts/argos-build.ps1 -QtDir 'C:/Qt/6.8.3/msvc2022_64' -VcpkgRoot 'C:/vcpkg'
#>
param(
    [string]$VcpkgRoot = 'C:/vcpkg',
    [string]$QtDir     = 'C:/Qt/6.8.3/msvc2022_64',
    [string]$Generator = 'Visual Studio 17 2022',
    [string]$Config    = 'Release',
    [switch]$Tests
)

$ErrorActionPreference = 'Stop'
$repo  = Split-Path -Parent $PSScriptRoot          # .../mayo
$build = Join-Path $repo 'build'

# Locate the CMake bundled with Visual Studio Build Tools (or fall back to PATH).
$cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
if (-not $cmake) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vs = & $vswhere -latest -products * -property installationPath
        $cand = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
        if (Test-Path $cand) { $cmake = $cand }
    }
}
if (-not $cmake) { throw 'cmake not found (install Visual Studio 2022 Build Tools with the C++ CMake component)' }

Write-Host "== Configuring Argos ==" -ForegroundColor Cyan
& $cmake -S $repo -B $build -G $Generator -A x64 `
    "-DCMAKE_TOOLCHAIN_FILE=$VcpkgRoot/scripts/buildsystems/vcpkg.cmake" `
    -DVCPKG_TARGET_TRIPLET=x64-windows `
    "-DCMAKE_PREFIX_PATH=$QtDir" `
    -DMayo_PostBuildCopyRuntimeDLLs=OFF `
    -DMayo_BuildConvCli=ON `
    -DArgos_BuildCoreTests=ON `
    -DMayo_BuildTests=OFF
if ($LASTEXITCODE -ne 0) { throw "configure failed ($LASTEXITCODE)" }

# mayo-conv is the headless offscreen renderer used by scripts/argos_report.py to
# put 3D view images into the measurement report.
Write-Host "== Building (mayo + argos-cli + mayo-conv + argos_core_test) ==" -ForegroundColor Cyan
& $cmake --build $build --config $Config --target mayo argos-cli mayo-conv argos_core_test --parallel
if ($LASTEXITCODE -ne 0) { throw "build failed ($LASTEXITCODE)" }

if ($Tests) {
    Write-Host "== Running argos_core unit tests ==" -ForegroundColor Cyan
    & (Join-Path $build "$Config/argos_core_test.exe")
    if ($LASTEXITCODE -ne 0) { throw "unit tests failed ($LASTEXITCODE)" }
}

Write-Host "== Done. Artifacts in $build/$Config ==" -ForegroundColor Green
