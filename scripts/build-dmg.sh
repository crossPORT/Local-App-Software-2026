#!/usr/bin/env bash
# Create a macOS .dmg from a cmake install prefix containing RocketBox.app
set -euo pipefail

INSTALL_PREFIX="${1:?install prefix}"
OUT_DIR="${2:?output dir}"
TAG="${3:-0.0.0}"

APP="$INSTALL_PREFIX/RocketBox.app"
BIN="$APP/Contents/MacOS/RocketBox"

if [[ ! -x "$BIN" ]]; then
  echo "Missing app binary: $BIN" >&2
  ls -laR "$APP" >&2 || true
  exit 1
fi

mkdir -p "$OUT_DIR"
OUT_DMG="$OUT_DIR/RocketBox-${TAG}-macos.dmg"
rm -f "$OUT_DMG"

hdiutil create -volname "RocketBox App" -srcfolder "$APP" -ov -format UDZO "$OUT_DMG"
echo "Created $OUT_DMG"
