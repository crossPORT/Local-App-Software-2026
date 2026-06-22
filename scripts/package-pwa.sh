#!/usr/bin/env bash
# Build the PWA and zip dist/ for offline distribution.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WEB="$ROOT/apps/web"
OUT="${1:-$ROOT/RocketBox-pwa.zip}"

# Relative output paths are anchored to the repo root (not apps/web/dist).
case "$OUT" in
  /*) ;;
  *) OUT="$ROOT/$OUT" ;;
esac

cd "$WEB"
npm ci
npm test
npm run build

rm -f "$OUT"
mkdir -p "$(dirname "$OUT")"
(cd dist && zip -r "$OUT" .)
echo "Created $OUT ($(du -h "$OUT" | cut -f1))"
