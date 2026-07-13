#!/usr/bin/env bash
# Layout invariants for @rocketbox/sdk (one transport surface).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

fail() { echo "FAIL: $1" >&2; exit 1; }

factory="$(rg -n 'new RocketBoxTransportImpl' sdks/typescript/src/create_transport.ts || true)"
echo "$factory" | grep -q 'RocketBoxTransportImpl' || fail "createRocketBoxTransport must construct RocketBoxTransportImpl"

if rg -n 'navigator\.usb|transferIn\(|transferOut\(|claimInterface\(' \
  sdks/typescript/src --glob '!**/transports/**' --glob '!**/webusb.d.ts'; then
  fail "WebUSB I/O must live under src/transports/"
fi

if rg -n 'navigator\.usb|transferIn\(|transferOut\(|claimInterface\(' apps/web/src; then
  fail "PWA must use @rocketbox/sdk for USB"
fi

n_transport="$(rg -n 'implements Transport' sdks/typescript/src | wc -l)"
[[ "$n_transport" -eq 1 ]] || fail "expected 1 Transport impl (SimTransport), got $n_transport"

n_impl="$(rg -n 'implements RocketBoxTransport' sdks/typescript/src | wc -l)"
[[ "$n_impl" -eq 1 ]] || fail "expected 1 RocketBoxTransport impl, got $n_impl"

rg -n 'implements Transport' sdks/typescript/src/transports/sim.ts >/dev/null \
  || fail "SimTransport must implement Transport"

echo "OK: @rocketbox/sdk layout"
