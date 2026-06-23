#!/usr/bin/env bash
# Build rocketbox.icns for the macOS app bundle (run on macOS only).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC_PNG="${ROOT}/apps/web/public/icon-512.png"
OUT_ICNS="${ROOT}/cmake/icons/rocketbox.icns"
ICONSET="${TMPDIR:-/tmp}/rocketbox.iconset.$$"
WORK_SRC="${TMPDIR:-/tmp}/rocketbox-rgba.$$"

cleanup() { rm -rf "$ICONSET" "$WORK_SRC"; }
trap cleanup EXIT

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "generate-macos-icon.sh: skip (macOS only)" >&2
    exit 0
fi

if [[ ! -f "$SRC_PNG" ]]; then
    echo "Missing source icon: $SRC_PNG" >&2
    exit 1
fi

mkdir -p "${ROOT}/cmake/icons"
mkdir -p "$ICONSET"

# iconutil requires RGBA PNGs; our source may be RGB if an old asset was committed.
sips -s format png "$SRC_PNG" --out "$WORK_SRC" >/dev/null
if ! sips -g hasAlpha "$WORK_SRC" 2>/dev/null | grep -q "yes"; then
    echo "Source icon lacks alpha; iconutil needs RGBA PNG (regenerate icon-512.png from favicon.svg)" >&2
    exit 1
fi

write_icon() {
    local name="$1"
    local size="$2"
    sips -z "$size" "$size" "$WORK_SRC" --out "${ICONSET}/${name}" >/dev/null
    local w h
    w="$(sips -g pixelWidth "${ICONSET}/${name}" | awk '/pixelWidth/ {print $2}')"
    h="$(sips -g pixelHeight "${ICONSET}/${name}" | awk '/pixelHeight/ {print $2}')"
    if [[ "$w" != "$size" || "$h" != "$size" ]]; then
        echo "Icon ${name} is ${w}x${h}, expected ${size}x${size}" >&2
        exit 1
    fi
}

write_icon "icon_16x16.png" 16
write_icon "icon_16x16@2x.png" 32
write_icon "icon_32x32.png" 32
write_icon "icon_32x32@2x.png" 64
write_icon "icon_128x128.png" 128
write_icon "icon_128x128@2x.png" 256
write_icon "icon_256x256.png" 256
write_icon "icon_256x256@2x.png" 512
write_icon "icon_512x512.png" 512
write_icon "icon_512x512@2x.png" 1024

iconutil -c icns "$ICONSET" -o "$OUT_ICNS"
echo "Wrote $OUT_ICNS"
