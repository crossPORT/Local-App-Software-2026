#!/usr/bin/env bash
# Build an AppImage from a cmake install prefix (Linux release only).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
INSTALL_PREFIX="${1:?install prefix}"
OUT_DIR="${2:?output dir}"
TAG="${3:-v0.0.0}"

mkdir -p "$OUT_DIR"
WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

APPDIR="$WORKDIR/AppDir"
mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/share/rocketbox"

cp -a "$INSTALL_PREFIX/bin/RocketBox" "$APPDIR/usr/bin/RocketBox"
cp -a "$INSTALL_PREFIX/share/rocketbox/"* "$APPDIR/usr/share/rocketbox/" 2>/dev/null || true

cat > "$APPDIR/AppRun" <<'EOF'
#!/bin/sh
HERE="$(dirname "$(readlink -f "$0")")"
export LD_LIBRARY_PATH="$HERE/usr/lib:$LD_LIBRARY_PATH"
exec "$HERE/usr/bin/RocketBox" "$@"
EOF
chmod +x "$APPDIR/AppRun"

mkdir -p "$APPDIR/usr/share/applications" "$APPDIR/usr/share/icons/hicolor/scalable/apps"
cp "$ROOT/apps/web/public/favicon.svg" "$APPDIR/usr/share/icons/hicolor/scalable/apps/rocketbox.svg"
cat > "$APPDIR/usr/share/applications/rocketbox.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=RocketBox App
Exec=RocketBox
Icon=rocketbox
Categories=Utility;
Terminal=false
EOF

LINUXDEPLOY="$WORKDIR/linuxdeploy-x86_64.AppImage"
curl -fsSL -o "$LINUXDEPLOY" \
  https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
chmod +x "$LINUXDEPLOY"

"$LINUXDEPLOY" --appdir "$APPDIR" --output appimage \
  --desktop-file="$APPDIR/usr/share/applications/rocketbox.desktop" \
  --icon-file="$APPDIR/usr/share/icons/hicolor/scalable/apps/rocketbox.svg" \
  --executable="$APPDIR/usr/bin/RocketBox"

APPIMAGE="$(find "$WORKDIR" -maxdepth 1 -name '*.AppImage' | head -1)"
if [[ -z "$APPIMAGE" ]]; then
  echo "AppImage build failed" >&2
  exit 1
fi

cp "$APPIMAGE" "$OUT_DIR/RocketBox-${TAG}-linux-x64.AppImage"
echo "Created $OUT_DIR/RocketBox-${TAG}-linux-x64.AppImage"
