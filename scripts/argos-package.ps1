<#
.SYNOPSIS
  Package Argos into a self-contained, redistributable Windows folder + zip.

.DESCRIPTION
  Assembles build/<Config> into dist/Argos: the GUI app (renamed Argos.exe), the
  headless argos-cli.exe, all runtime DLLs and Qt plugin folders that were
  deployed next to the build output, an optional bundled Korean font, plus README
  and license. Produces dist/Argos-win64.zip.

  Run scripts/argos-build.ps1 first. DLLs are expected to already sit next to the
  build output (vcpkg app-local deployment + windeployqt). If Qt DLLs are missing,
  this script runs windeployqt when it can find it.

.EXAMPLE
  pwsh -File scripts/argos-package.ps1
#>
param(
    [string]$Config = 'Release',
    [string]$QtDir  = 'C:/Qt/6.8.3/msvc2022_64'
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$rel  = Join-Path $repo "build/$Config"
$dist = Join-Path $repo 'dist/Argos'

if (-not (Test-Path (Join-Path $rel 'mayo.exe'))) {
    throw "mayo.exe not found in $rel - run scripts/argos-build.ps1 first"
}

Write-Host "== Cleaning $dist ==" -ForegroundColor Cyan
if (Test-Path $dist) { Remove-Item -Recurse -Force $dist }
New-Item -ItemType Directory -Force -Path $dist | Out-Null

# Executables: rebrand the GUI exe as Argos.exe; keep argos-cli.exe as-is.
Copy-Item (Join-Path $rel 'mayo.exe') (Join-Path $dist 'Argos.exe') -Force
if (Test-Path (Join-Path $rel 'argos-cli.exe')) {
    Copy-Item (Join-Path $rel 'argos-cli.exe') $dist -Force
}
# mayo-conv.exe: headless offscreen renderer used by the PPTX report generator.
if (Test-Path (Join-Path $rel 'mayo-conv.exe')) {
    Copy-Item (Join-Path $rel 'mayo-conv.exe') $dist -Force
}

# Ensure Qt is deployed next to Argos.exe (idempotent). windeployqt prints
# harmless warnings (e.g. missing dxcompiler.dll) to stderr; don't let those abort
# packaging. The runtime DLLs from build/<Config> below are the real source.
$windeployqt = Join-Path $QtDir 'bin/windeployqt.exe'
if (Test-Path $windeployqt) {
    $prevEAP = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    & $windeployqt --release --no-translations --no-compiler-runtime (Join-Path $dist 'Argos.exe') *>&1 | Out-Null
    $ErrorActionPreference = $prevEAP
}

# Runtime DLLs that were app-local-deployed next to the build output (OCCT TK*,
# Qt6*, vcpkg deps like freetype/zlib/...).
Get-ChildItem $rel -Filter *.dll | ForEach-Object { Copy-Item $_.FullName $dist -Force }

# Qt plugin folders (windeployqt creates these next to the build exe too).
foreach ($plug in @('platforms','styles','tls','iconengines','imageformats','generic','networkinformation','platforminputcontexts')) {
    $src = Join-Path $rel $plug
    if (Test-Path $src) { Copy-Item $src $dist -Recurse -Force }
}

# Optional bundled fonts (Korean UI). If scripts/argos-fetch-font.ps1 was run, a
# fonts/ folder exists next to the exe and Argos loads *.ttf/*.otf at startup.
$fontSrc = Join-Path $repo 'resources/fonts'
if (Test-Path $fontSrc) {
    Copy-Item $fontSrc (Join-Path $dist 'fonts') -Recurse -Force
}

# Docs + license notices (keep Mayo's permissive license).
foreach ($doc in @('README.md','README.en.md','LICENSE.txt')) {
    $src = Join-Path $repo $doc
    if (Test-Path $src) { Copy-Item $src $dist -Force }
}

# Zip it.
$zip = Join-Path $repo 'dist/Argos-win64.zip'
if (Test-Path $zip) { Remove-Item -Force $zip }
Compress-Archive -Path (Join-Path $dist '*') -DestinationPath $zip -Force

$size = [math]::Round((Get-Item $zip).Length / 1MB, 1)
$count = (Get-ChildItem $dist -Recurse -File).Count
Write-Host "== Packaged $count files into $dist ==" -ForegroundColor Green
Write-Host "== Zip: $zip ($size MB) ==" -ForegroundColor Green
