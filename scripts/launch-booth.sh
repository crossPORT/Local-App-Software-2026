#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP="$ROOT/build/apps/wx/RocketBox"

if [[ ! -x "$APP" ]]; then
  echo "Build first: cmake -S $ROOT -B $ROOT/build && cmake --build $ROOT/build -j" >&2
  exit 1
fi

pkill -f 'RocketBox --port' 2>/dev/null || true
sleep 0.3

CONFIG="${ROCKETBOX_CONFIG:-$ROOT/booth-shared.conf}"
CONFIG_ARGS=()
if [[ -f "$CONFIG" ]]; then
  CONFIG_ARGS=(--config "$CONFIG")
  echo "Using config: $CONFIG"
fi

echo "Launching booth pair — two windows (Receiver port 0, Sender port 1)"
cd "$ROOT"
"$APP" "${CONFIG_ARGS[@]}" --port 0 &
sleep 0.4
"$APP" "${CONFIG_ARGS[@]}" --port 1 &
echo "Done. Port 0 = Receive first, then Send on port 1."
