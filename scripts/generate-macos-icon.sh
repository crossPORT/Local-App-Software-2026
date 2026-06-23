#!/usr/bin/env bash
# Build rocketbox.icns for the macOS app bundle (run on macOS only).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC_PNG="${ROOT}/apps/web/public/icon-512.png"
OUT_ICNS="${ROOT}/cmake/icons/rocketbox.icns"

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "generate-macos-icon.sh: skip (macOS only)" >&2
    exit 0
fi

if [[ ! -f "$SRC_PNG" ]]; then
    echo "Missing source icon: $SRC_PNG" >&2
    exit 1
fi

if ! command -v png2icns >/dev/null 2>&1; then
    echo "png2icns not found; install with: brew install libicns" >&2
    exit 1
fi

mkdir -p "${ROOT}/cmake/icons"
rm -f "$OUT_ICNS"
png2icns "$OUT_ICNS" "$SRC_PNG"
echo "Wrote $OUT_ICNS"
