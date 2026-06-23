# Fail if the NSIS installer does not install a self-contained RocketBox tree.
param(
    [Parameter(Mandatory = $true)]
    [string]$SetupExe,
    [string]$TestRoot = "$env:RUNNER_TEMP\nsis-verify"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $SetupExe)) {
    Write-Error "Missing NSIS installer: $SetupExe"
}

$installRoot = Join-Path $TestRoot "RocketBox"
Remove-Item -Recurse -Force $TestRoot -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $installRoot | Out-Null

Write-Host "Silent-installing NSIS package to $installRoot"
$proc = Start-Process -FilePath $SetupExe -ArgumentList @("/S", "/D=$installRoot") -Wait -PassThru
if ($proc.ExitCode -ne 0) {
    Write-Error "NSIS silent install failed with exit code $($proc.ExitCode)"
}

$prefix = $installRoot
$exe = Join-Path $prefix "bin\RocketBox.exe"
if (-not (Test-Path $exe)) {
    $found = Get-ChildItem -Path $installRoot -Recurse -Filter "RocketBox.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($found) {
        $prefix = $found.Directory.Parent.FullName
        $exe = $found.FullName
    }
}

if (-not (Test-Path $exe)) {
    Write-Error "NSIS install did not produce RocketBox.exe under $installRoot"
}

Write-Host "NSIS installed RocketBox.exe at $exe"
& "$PSScriptRoot/verify-windows-install.ps1" -InstallPrefix $prefix
