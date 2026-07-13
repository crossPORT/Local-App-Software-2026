# Download official Wintun amd64 DLL into DestDir (default: beside this script's ../).
param(
    [Parameter(Mandatory = $true)]
    [string]$DestDir,
    [string]$Version = "0.14.1"
)

$ErrorActionPreference = "Stop"
New-Item -ItemType Directory -Force -Path $DestDir | Out-Null
$zip = Join-Path $env:TEMP "wintun-$Version.zip"
$url = "https://www.wintun.net/builds/wintun-$Version.zip"
Write-Host "Downloading $url"
Invoke-WebRequest -Uri $url -OutFile $zip
$extract = Join-Path $env:TEMP "wintun-$Version"
if (Test-Path $extract) { Remove-Item -Recurse -Force $extract }
Expand-Archive -Path $zip -DestinationPath $extract
$dll = Join-Path $extract "wintun\bin\amd64\wintun.dll"
if (-not (Test-Path $dll)) {
    throw "wintun.dll not found in archive at $dll"
}
Copy-Item -Force $dll (Join-Path $DestDir "wintun.dll")
Write-Host "Installed wintun.dll -> $DestDir"
