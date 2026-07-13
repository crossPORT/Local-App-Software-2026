#!/bin/bash
# Launch rocketbox-tunnel-tray with the desktop session bus (required for GNOME tray).
set -euo pipefail

BIN="${ROCKETBOX_TUNNEL_TRAY_PATH:-}"
if [[ -z "$BIN" ]]; then
  HERE="$(cd "$(dirname "$0")" && pwd)"
  ROOT="$(cd "$HERE/../.." && pwd)"
  for c in \
    "$ROOT/build/apps/tunnel-tray/rocketbox-tunnel-tray" \
    "$HERE/rocketbox-tunnel-tray" \
    /usr/bin/rocketbox-tunnel-tray; do
    if [[ -x "$c" ]]; then BIN="$c"; break; fi
  done
fi
if [[ -z "${BIN:-}" || ! -x "$BIN" ]]; then
  echo "rocketbox-tunnel-tray not found (build it, or set ROCKETBOX_TUNNEL_TRAY_PATH)" >&2
  exit 1
fi

# Pull env from a live desktop process when launched from a bare shell / IDE.
if [[ -z "${DBUS_SESSION_BUS_ADDRESS:-}" ]]; then
  for name in gnome-shell gsd-xsettings; do
    pid=$(pgrep -u "$USER" -n "$name" 2>/dev/null || true)
    if [[ -n "$pid" && -r "/proc/$pid/environ" ]]; then
      while IFS= read -r -d '' kv; do
        case "$kv" in
          DISPLAY=*|WAYLAND_DISPLAY=*|DBUS_SESSION_BUS_ADDRESS=*|XDG_RUNTIME_DIR=*|XDG_CURRENT_DESKTOP=*|XDG_SESSION_TYPE=*)
            export "$kv"
            ;;
        esac
      done < "/proc/$pid/environ"
      break
    fi
  done
fi

export DISPLAY="${DISPLAY:-:0}"
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
export DBUS_SESSION_BUS_ADDRESS="${DBUS_SESSION_BUS_ADDRESS:-unix:path=${XDG_RUNTIME_DIR}/bus}"
# Prefer X11 under GNOME Wayland for wx tray; allow override.
if [[ -z "${GDK_BACKEND:-}" && "${XDG_SESSION_TYPE:-}" == "wayland" ]]; then
  export GDK_BACKEND=x11
fi

exec "$BIN" "$@"
