#!/usr/bin/env bash
# Build tab/PWA/desktop icons from the orange rocket mark used in the app header.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MARK="${ROOT}/apps/web/public/rocketbox-mark.png"
PUBLIC="${ROOT}/apps/web/public"
ICONDIR="${ROOT}/cmake/icons"

if [[ ! -f "$MARK" ]]; then
    echo "Missing brand mark: $MARK" >&2
    exit 1
fi

compose_mark_icon() {
    local size="$1"
    local out="$2"
    local inner=$((size * 78 / 100))
    local radius=$((size / 5))
    local last=$((size - 1))
    convert -size "${size}x${size}" xc:none \
        -fill white \
        -draw "roundrectangle 0,0 ${last},${last} ${radius},${radius}" \
        \( "$MARK" -resize "${inner}x${inner}" \) \
        -gravity center -composite "$out"
}

mkdir -p "$ICONDIR"
compose_mark_icon 512 "${PUBLIC}/icon-512.png"
compose_mark_icon 192 "${PUBLIC}/icon-192.png"
compose_mark_icon 64 "${PUBLIC}/favicon.png"
compose_mark_icon 256 "${ICONDIR}/rocketbox-256.png"
convert "${PUBLIC}/icon-512.png" -compress none -define icon:auto-resize=256,48,32,16 \
    "${ICONDIR}/rocketbox.ico"
convert "${PUBLIC}/icon-512.png" -compress none -define icon:auto-resize=48,32,16 \
    "${ICONDIR}/rocketbox-installer.ico"

echo "Updated PWA and desktop icons from rocketbox-mark.png"
