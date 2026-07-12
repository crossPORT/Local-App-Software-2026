#!/usr/bin/env bash
# Fail if a second TypeScript device-connect path reappears.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

fail() { echo "FAIL: $1" >&2; exit 1; }

if rg -n 'FabricUsbSession|FabricLink|RocketBxSystemsDirectory|rocketbx_usb|rocketbx_systems|rocketbx_open|rocketbx_stubs' \
  sdks/typescript/src apps/web/src; then
  fail "deleted HW stack symbols found"
fi

factory="$(rg -n 'new (FabricUsbSession|SdkFabricTransport|RocketBoxTransport)' sdks/typescript/src/fabric/create_transport.ts || true)"
echo "$factory" | grep -q 'RocketBoxTransport' || fail "factory must construct RocketBoxTransport"
echo "$factory" | grep -qE 'FabricUsbSession|SdkFabricTransport' && fail "factory must not construct deleted classes" || true

if rg -n 'navigator\.usb|transferIn\(|transferOut\(|claimInterface\(' \
  sdks/typescript/src --glob '!**/transports/**' --glob '!**/webusb.d.ts'; then
  fail "WebUSB I/O outside transports/"
fi

if rg -n 'navigator\.usb|transferIn\(|transferOut\(|claimInterface\(' apps/web/src; then
  fail "PWA must not open USB"
fi

transport_impls="$(rg -c 'implements Transport' sdks/typescript/src || true)"
# expect 2 files (usb + sim) — count matching lines
n_transport="$(rg -n 'implements Transport' sdks/typescript/src | wc -l)"
[[ "$n_transport" -eq 2 ]] || fail "expected 2 Transport impls, got $n_transport"

n_fabric="$(rg -n 'implements FabricTransport' sdks/typescript/src | wc -l)"
[[ "$n_fabric" -eq 1 ]] || fail "expected 1 FabricTransport impl, got $n_fabric"

if rg -n '\bintelli_|\bSdkFabric\b|IntelliConnex' sdks/typescript/src apps/web/src --glob '*.ts' --glob '*.tsx'; then
  fail "company/brand leftovers in TS identifiers/comments"
fi

echo "OK: single TypeScript device-connect path"
