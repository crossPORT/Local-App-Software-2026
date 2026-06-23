#!/usr/bin/env bash
# Build rocketbox.icns for the macOS app bundle (run on macOS only).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC_PNG="${ROOT}/apps/web/public/icon-512.png"
OUT_ICNS="${ROOT}/cmake/icons/rocketbox.icns"
ICONSET="${TMPDIR:-/tmp}/rocketbox.iconset.$$"

cleanup() { rm -rf "$ICONSET"; }
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

# iconutil requires a *.iconset folder and exact icon_WxH[@2x].png names.
write_icon() {
    local name="$1"
    local size="$2"
    sips -z "$size" "$size" "$SRC_PNG" --out "${ICONSET}/${name}" >/dev/null
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
