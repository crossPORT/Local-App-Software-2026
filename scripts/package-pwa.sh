#!/usr/bin/env bash
# Build the PWA and zip dist/ for offline distribution.
# Usage: package-pwa.sh [output.zip] [semver-for-package.json]
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WEB="$ROOT/apps/web"
OUT="${1:-$ROOT/RocketBox-pwa.zip}"
PWA_VERSION="${2:-}"

# Relative output paths are anchored to the repo root (not apps/web/dist).
case "$OUT" in
  /*) ;;
  *) OUT="$ROOT/$OUT" ;;
esac

if [[ -n "$PWA_VERSION" ]]; then
  node -e "
    const fs = require('fs');
    const path = '$WEB/package.json';
    const pkg = JSON.parse(fs.readFileSync(path, 'utf8'));
    pkg.version = '$PWA_VERSION';
    fs.writeFileSync(path, JSON.stringify(pkg, null, 2) + '\n');
  "
fi

# Embed release tag in the PWA Settings UI (Vite exposes VITE_*).
if [[ -z "${VITE_ROCKETBOX_RELEASE_TAG:-}" ]]; then
  if [[ -n "${ROCKETBOX_RELEASE_TAG:-}" ]]; then
    export VITE_ROCKETBOX_RELEASE_TAG="$ROCKETBOX_RELEASE_TAG"
  elif [[ -n "$PWA_VERSION" ]]; then
    export VITE_ROCKETBOX_RELEASE_TAG="v$PWA_VERSION"
  else
    export VITE_ROCKETBOX_RELEASE_TAG="$(git -C "$ROOT" describe --tags --always --dirty 2>/dev/null || echo dev)"
  fi
fi

cd "$WEB"
npm ci
npm test
npm run build

rm -f "$OUT"
mkdir -p "$(dirname "$OUT")"
(cd dist && zip -r "$OUT" .)
echo "Created $OUT ($(du -h "$OUT" | cut -f1))"
