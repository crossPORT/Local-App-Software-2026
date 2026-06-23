#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT/apps/web"
if [[ ! -d node_modules ]]; then
  npm install
fi
exec npm run dev -- --host 0.0.0.0 --port "${1:-8080}"
