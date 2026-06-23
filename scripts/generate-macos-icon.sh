#!/usr/bin/env bash
# Build rocketbox.icns for the macOS app bundle (run on macOS only).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC_PNG="${ROOT}/apps/web/public/icon-512.png"
OUT_ICNS="${ROOT}/cmake/icons/rocketbox.icns"
ICONSET="$(mktemp -d)"

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
for size in 16 32 128 256 512; do
    sips -z "$size" "$size" "$SRC_PNG" --out "${ICONSET}/icon_${size}x${size}.png" >/dev/null
    double=$((size * 2))
    if [[ "$double" -le 1024 ]]; then
        sips -z "$double" "$double" "$SRC_PNG" --out "${ICONSET}/icon_${size}x${size}@2x.png" >/dev/null
    fi
done

iconutil -c icns "$ICONSET" -o "$OUT_ICNS"
echo "Wrote $OUT_ICNS"
