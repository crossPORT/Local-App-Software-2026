# Fail if the Windows install prefix is missing bundled wx/libusb DLLs.
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

$Libusb = Join-Path $Bin "libusb-1.0.dll"
if (-not (Test-Path $Libusb)) {
    Write-Error "Missing bundled libusb-1.0.dll in $Bin"
}

$WxDlls = Get-ChildItem -Path $Bin -Filter "wx*.dll" -ErrorAction SilentlyContinue
if (-not $WxDlls) {
    Write-Error "Missing bundled wxWidgets DLLs in $Bin"
}

Write-Host "Windows install looks self-contained:"
Get-ChildItem -Path $Bin -Filter "*.dll" | ForEach-Object { Write-Host "  $($_.Name)" }
