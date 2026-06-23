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

$Required = @("libusb-1.0.dll", "liblzma.dll")
foreach ($name in $Required) {
    $path = Join-Path $Bin $name
    if (-not (Test-Path $path)) {
        Write-Error "Missing bundled $name in $Bin"
    }
}

$Zlib = @(
    (Join-Path $Bin "z.dll")
    (Join-Path $Bin "zlib1.dll")
) | Where-Object { Test-Path $_ }
if (-not $Zlib) {
    Write-Error "Missing bundled z.dll or zlib1.dll in $Bin"
}

$WxDlls = Get-ChildItem -Path $Bin -Filter "wx*.dll" -ErrorAction SilentlyContinue
if (-not $WxDlls) {
    Write-Error "Missing bundled wxWidgets DLLs in $Bin"
}

$AllDlls = Get-ChildItem -Path $Bin -Filter "*.dll" -ErrorAction SilentlyContinue
if ($AllDlls.Count -lt 10) {
    Write-Error "Expected at least 10 bundled DLLs in $Bin, found $($AllDlls.Count)"
}

Write-Host "Windows install looks self-contained ($($AllDlls.Count) DLL(s)):"
$AllDlls | ForEach-Object { Write-Host "  $($_.Name)" }
