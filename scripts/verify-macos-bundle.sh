#!/usr/bin/env bash
# Fail if RocketBox.app still references Homebrew/local absolute dylib paths.
set -euo pipefail

APP="${1:?path to RocketBox.app}"
BIN="$APP/Contents/MacOS/RocketBox"

if [[ ! -f "$BIN" ]]; then
  echo "Missing binary: $BIN" >&2
  exit 1
fi

check_file() {
  local file="$1"
  local bad
  bad="$(otool -L "$file" | awk '/^\t/ {print $1}' | grep -E '^(/opt/homebrew|/usr/local)' || true)"
  if [[ -n "$bad" ]]; then
    echo "Unbundled dependency in $file:" >&2
    echo "$bad" >&2
    return 1
  fi
  return 0
}

failed=0
while IFS= read -r dep; do
  [[ -z "$dep" ]] && continue
  if [[ "$dep" == /opt/homebrew/* || "$dep" == /usr/local/* ]]; then
    echo "Unbundled dependency in $BIN: $dep" >&2
    failed=1
  fi
done < <(otool -L "$BIN" | awk '/^\t/ {print $1}')

if [[ -d "$APP/Contents/Frameworks" ]]; then
  while IFS= read -r fw; do
    check_file "$fw" || failed=1
  done < <(find "$APP/Contents/Frameworks" -type f \( -name '*.dylib' -o -name '*.so' \))
fi

if [[ "$failed" -ne 0 ]]; then
  echo "macOS bundle is not self-contained" >&2
  otool -L "$BIN" >&2 || true
  exit 1
fi

if ! codesign --verify --deep "$APP" 2>/dev/null; then
  echo "macOS bundle failed codesign verification: $APP" >&2
  codesign --verify --deep --verbose=2 "$APP" >&2 || true
  exit 1
fi

echo "macOS bundle looks self-contained and signed: $APP"
