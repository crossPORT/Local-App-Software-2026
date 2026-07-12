# TypeScript SDK agent guidance (`sdks/typescript`)

`@rocketbox/sdk` owns **all** browser USB I/O and wire protocol for the PWA.

## Public vocabulary

Same as root [AGENTS.md](../../AGENTS.md): **RocketBox**, **port**, **session**, **transfer**, plus EP4 verb **switch**.

## Authoritative HW contract (this box)

C++ mirrors working TS HW; names are aligned across SDKs:

1. **transfer** — ROCKETBX on data OUT `0x02` / IN `0x81` (announce, session, payload)
2. **switch** on EP4 (`usb_switch.ts` / C++ `switch_port_core`) — 16-byte word, dest in low nibble; **not** used on the session/transfer send path. Connect/reset clears with dest=0.

Topology sketch: [rocketbox-crossport-fabric-v1.html](../../Agentic%20Software%20documents/rocketbox-crossport-fabric-v1.html)

**Not authoritative for this hardware:** `IntelliConnex_*_Requirement.md` Session ATTACH / LIST / CONNECT / EP3 control framing. Do not gate connect on ATTACH.

## One SDK — one device path

`createFabricTransport({ simulate, port })` always returns **`RocketBoxTransport`**.

| Mode | USB messaging |
|------|----------------|
| real (default) | WebUSB open/claim → ROCKETBX transfer; switch clear on connect |
| `simulate=true` | `SimTransport` / WS daemon (sim may still speak Session control) |

**Forbidden:** a second browser USB stack or PWA WebUSB. `transferIn` / `transferOut` / `claimInterface` live only under `src/transports/`.

## Layout

- `src/transports/` — WebUSB open, switch, ROCKETBX transfer
- `src/fabric/` — `RocketBoxTransport`, codecs, factory (legacy folder name)
- Sim / Session modules remain for `simulate=true` only

## Rules

- No UI / React / Vite imports
- Every source file ≤200 lines
- Product naming: **RocketBox** / `rocketbox_*`
- Gate: `scripts/check-single-ts-usb.sh`

## Tests

`cd apps/web && npm test` (vitest alias into SDK).
