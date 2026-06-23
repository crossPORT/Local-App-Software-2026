#!/usr/bin/env bash
# Regenerate cmake/icons from apps/web/public (run after favicon.svg changes).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SVG="${ROOT}/apps/web/public/favicon.svg"
PNG512="${ROOT}/apps/web/public/icon-512.png"
PNG192="${ROOT}/apps/web/public/icon-192.png"
ICONDIR="${ROOT}/cmake/icons"

mkdir -p "$ICONDIR"

convert -background none "$SVG" -resize 512x512 "$PNG512"
convert -background none "$SVG" -resize 192x192 "$PNG192"
cp "$SVG" "${ICONDIR}/rocketbox.svg"
convert "$PNG512" -resize 256x256 "${ICONDIR}/rocketbox-256.png"
# App/window icon (installed next to RocketBox.exe).
convert "$PNG512" -compress none -define icon:auto-resize=48,32,16 "${ICONDIR}/rocketbox.ico"
# NSIS embeds icons in the setup.exe; keep small (no 256x256 BMP layer).
convert "$PNG512" -compress none -define icon:auto-resize=48,32,16 "${ICONDIR}/rocketbox-installer.ico"

echo "Updated PWA PNGs and cmake/icons"
