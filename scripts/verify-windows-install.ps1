# Fail if the Windows install prefix is missing bundled runtime DLLs.
param(
    [Parameter(Mandatory = $true)]
    [string]$InstallPrefix
)

$ErrorActionPreference = "Stop"
$Bin = Join-Path $InstallPrefix "bin"
$Exe = Join-Path $Bin "RocketBox.exe"

if (-not (Test-Path $Exe)) {
    Write-Error "Missing binary: $Exe"
}

$Required = @("libusb-1.0.dll")
foreach ($name in $Required) {
    $path = Join-Path $Bin $name
    if (-not (Test-Path $path)) {
        Write-Error "Missing bundled $name in $Bin"
    }
}

$WxDlls = Get-ChildItem -Path $Bin -Filter "wx*.dll" -ErrorAction SilentlyContinue
if (-not $WxDlls) {
    Write-Error "Missing bundled wxWidgets DLLs in $Bin"
}

$AllDlls = Get-ChildItem -Path $Bin -Filter "*.dll" -ErrorAction SilentlyContinue
if ($AllDlls.Count -lt 3) {
    Write-Error "Expected at least 3 bundled DLLs in $Bin, found $($AllDlls.Count)"
}

Write-Host "Windows install looks self-contained ($($AllDlls.Count) DLL(s)):"
$AllDlls | ForEach-Object { Write-Host "  $($_.Name)" }
