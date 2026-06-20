<#
.SYNOPSIS
  Download a Korean UI font (Noto Sans KR, SIL OFL) into resources/fonts so the
  packaged Argos renders Hangul on systems without a Korean system font.

.DESCRIPTION
  Best-effort: if the download fails, Argos still renders Korean using the system
  font on Korean Windows. The font is NOT committed to the repo; this script
  fetches it on demand. Argos loads any .ttf/.otf placed in a "fonts" folder next
  to the executable at startup (see src/app/main.cpp).
#>
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$dst  = Join-Path $repo 'resources/fonts'
New-Item -ItemType Directory -Force -Path $dst | Out-Null

$out = Join-Path $dst 'NotoSansKR.ttf'
# Static weight (Regular) from the Google Fonts repository (SIL OFL 1.1).
$urls = @(
    'https://github.com/google/fonts/raw/main/ofl/notosanskr/NotoSansKR%5Bwght%5D.ttf',
    'https://github.com/notofonts/noto-cjk/raw/main/Sans/OTF/Korean/NotoSansKR-Regular.otf'
)

foreach ($u in $urls) {
    try {
        Write-Host "Downloading $u ..." -ForegroundColor Cyan
        Invoke-WebRequest -Uri $u -OutFile $out -UseBasicParsing
        if ((Get-Item $out).Length -gt 100000) {
            Write-Host "Saved $out ($([math]::Round((Get-Item $out).Length/1MB,1)) MB)" -ForegroundColor Green
            return
        }
    }
    catch {
        Write-Warning "failed: $($_.Exception.Message)"
    }
}

Write-Warning "Could not fetch a Korean font. Argos will fall back to the system font (Hangul still renders on Korean Windows)."
