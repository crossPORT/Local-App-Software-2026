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
  # package.json needs semver; 1.0.0.waterloo -> 1.0.0-waterloo
  PWA_VERSION="$(python3 - <<PY
import re, sys
v = """$PWA_VERSION"""
m = re.match(r"^(\d+\.\d+\.\d+)(?:[.\-](.+))?$", v)
if m and m.group(2):
    print(f"{m.group(1)}-{m.group(2).replace('.', '-')}")
else:
    print(v)
PY
)"
  node -e "
    const fs = require('fs');
    const path = '$WEB/package.json';
    const pkg = JSON.parse(fs.readFileSync(path, 'utf8'));
    pkg.version = '$PWA_VERSION';
    fs.writeFileSync(path, JSON.stringify(pkg, null, 2) + '\n');
  "
fi

cd "$WEB"
npm ci
npm test
npm run build

rm -f "$OUT"
mkdir -p "$(dirname "$OUT")"
(cd dist && zip -r "$OUT" .)
echo "Created $OUT ($(du -h "$OUT" | cut -f1))"
