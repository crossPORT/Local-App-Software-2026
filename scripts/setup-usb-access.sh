#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RULE_SRC="$ROOT/99-rocketbox-usb.rules"
RULE_DST="/etc/udev/rules.d/99-rocketbox-usb.rules"
LEGACY_DST="/etc/udev/rules.d/99-sls-fabric-usb.rules"

if [[ ! -f "$RULE_SRC" ]]; then
  echo "Missing $RULE_SRC" >&2
  exit 1
fi

if [[ "${EUID}" -ne 0 ]]; then
  exec sudo "$0" "$@"
fi

install -m 644 "$RULE_SRC" "$RULE_DST"
# Keep legacy filename in sync for older install docs.
install -m 644 "$RULE_SRC" "$LEGACY_DST"
udevadm control --reload-rules
udevadm trigger

echo "USB access rule installed. Unplug and replug the RocketBox USB cable."
