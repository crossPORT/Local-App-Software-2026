#!/usr/bin/env bash
# Regenerate desktop/packaging icons from the tray outline mark, and PWA icons
# from apps/web/public/favicon.svg.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SVG="${ROOT}/apps/web/public/favicon.svg"
PNG512="${ROOT}/apps/web/public/icon-512.png"
PNG192="${ROOT}/apps/web/public/icon-192.png"
ICONDIR="${ROOT}/cmake/icons"

mkdir -p "$ICONDIR"

# PWA / web favicon assets (emoji rocket).
convert -background none "$SVG" -resize 512x512 "$PNG512"
convert -background none "$SVG" -resize 192x192 "$PNG192"

# Desktop .exe / shortcuts / Linux menu: same outline R as the tunnel tray.
python3 "${ROOT}/scripts/generate-tray-icons.py" --out-dir "$ICONDIR"
MARK512="${ICONDIR}/rocketbox-mark-512.png"

# App/window icon (installed next to RocketBox.exe / tray).
convert "$MARK512" -compress none -define icon:auto-resize=256,48,32,16 \
  "${ICONDIR}/rocketbox.ico"
# NSIS embeds icons in setup.exe; keep small (no 256x256 BMP layer).
convert "$MARK512" -compress none -define icon:auto-resize=48,32,16 \
  "${ICONDIR}/rocketbox-installer.ico"

# Approximate SVG of the mark (pixel-perfect assets come from the PNG).
cat > "${ICONDIR}/rocketbox.svg" <<'EOF'
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512">
  <g fill="none" stroke="#161C26" stroke-width="36">
    <rect x="51" y="51" width="410" height="410"/>
  </g>
  <g fill="#161C26">
    <rect x="136" y="136" width="48" height="240"/>
    <rect x="136" y="136" width="240" height="48"/>
    <rect x="328" y="136" width="48" height="115"/>
    <rect x="136" y="203" width="240" height="48"/>
    <polygon points="184,251 328,376 280,376 184,290"/>
  </g>
</svg>
EOF

echo "Updated PWA PNGs and cmake/icons (tray mark for .exe/shortcuts)"
