# Locate the CPack NSIS installer under build/ (any RocketBox-*.exe except RocketBox.exe).
param(
    [string]$BuildDir = "build",
    [string]$ExpectedTag = ""
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $BuildDir)) {
    Write-Error "Missing build directory: $BuildDir"
}

Write-Host "Installers/exes under ${BuildDir}:"
Get-ChildItem -Path $BuildDir -Filter '*.exe' -File -ErrorAction SilentlyContinue |
    ForEach-Object { Write-Host "  $($_.Name) ($($_.Length) bytes)" }

$candidates = Get-ChildItem -Path $BuildDir -Filter '*.exe' -File -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -like 'RocketBox-*' -and $_.Name -ne 'RocketBox.exe' } |
    Sort-Object Length -Descending

if (-not $candidates) {
    Write-Error "No NSIS installer found in ${BuildDir} (expected RocketBox-<tag>-setup.exe)"
}

$setup = $candidates[0]
if ($ExpectedTag -and $setup.Name -notlike "RocketBox-${ExpectedTag}-setup.exe") {
    Write-Error "Installer name does not match release tag. Expected RocketBox-${ExpectedTag}-setup.exe, found $($setup.Name)"
}

$setup.FullName
