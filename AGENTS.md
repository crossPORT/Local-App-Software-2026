# Agent onboarding

Read this first when working in this repository. Goal: USB file transfer to/from **RocketBox** hardware (`1772:0006`) using **RocketBox App** (wx + PWA) and CLI test tools.

## Public vocabulary (locked)

Use only these product nouns in APIs, tools, and docs:

| Noun | Meaning |
|------|---------|
| **RocketBox** | Product, app, SDK (`@rocketbox/sdk`, `RocketBox` binary) |
| **port** | User-facing port 1–4 (and libusb sort index where needed) |
| **session** | Handshake: announce / offer / accept / ready |
| **transfer** | ROCKETBX file/bytes on the data path |

EP4 control verb: **switch** — link this port to dest 1–4, or dest 0 to clear. (`switch_port_core`, `usb-switch`, TS `writeSwitch` / `switchPort`).

Not public product nouns (do not invent new ones; prefer the table above): ~~crossbar~~, ~~Mode 11~~, ~~IntelliConnex~~, ~~booth~~, ~~fabric~~, ~~Attach~~ (not the HW contract).

Wire magic **ROCKETBX** stays as the on-wire header constant only.

## Read order

1. **This file** — repo layout, build, conventions
2. **[docs/DEV-DEMO.md](docs/DEV-DEMO.md)** — run/test with **simulated hardware** or **real USB** (PWA, wx, tunnel)
3. **[sdks/typescript/AGENTS.md](sdks/typescript/AGENTS.md)** — when touching `@rocketbox/sdk` / WebUSB / wire
4. **[apps/web/AGENTS.md](apps/web/AGENTS.md)** — when touching the PWA (UI only; USB via SDK)
5. **[docs/PROTOCOL.md](docs/PROTOCOL.md)** — wire format (if editing `sdks/cpp/core/`)
6. **[docs/DEPLOYMENT.md](docs/DEPLOYMENT.md)** — CI, release installers, PWA zip on tags

## 30-second context

- **This repo:** CMake monorepo — one native engine under `sdks/cpp/` (`core` + `sim` + `session` + public `rocketbox_sdk`) + `apps/wx/` + `apps/web/` + `sdks/typescript/` + `tools/` + `rocketbox-tunnel/`
- **Engine:** `sdks/cpp/core` — async send, ring-buffer receive, `ROCKETBX` header; session orchestration in `sdks/cpp/session`
- **C++ SDK:** `create_rocketbox_transport(usb|sim, port)` → `RocketBoxTransport`; apps use `SessionOrchestrator` on that transport (`rocketbox/session_orchestrator.h`). No Attach.
- **TypeScript SDK:** `@rocketbox/sdk` — PWA uses `createRocketBoxTransport`
- **Simulation:** product device is `simulated-hardware/` (TCP 1772 + WS 1773); in-process lib is `sdks/cpp/sim`
- **HW USB:** ROCKETBX on data EPs + **switch** on EP4. Session ATTACH is not the HW contract.
- **Do not** start the GUI server for the user; they run binaries themselves

## Build (always verify changes)

```bash
cmake -S . -B build
cmake --build build -j
```

All targets must compile:

- `rocketbox_usb_core`, `rocketbox_usb_sim`, `rocketbox_session`, `rocketbox_sdk`
- `rocketbox` → **RocketBox** (wx), `usb-probe`, `usb-loopback-test`, `usb-switch`, `rocketbox-cli`, `session-test`
- `rocketbox-tunnel` (Linux)

## File map

| Path | Role |
|------|------|
| `sdks/cpp/core/` | USB engine (`rocketbox_usb_core`) |
| `sdks/cpp/sim/` | In-process USB sim (`rocketbox_usb_sim`) for CTest / session |
| `sdks/cpp/session/` | Shared session layer (`rocketbox_session`) |
| `sdks/cpp/include/rocketbox/sdk.h` | Public C++ SDK (`RocketBoxTransport`) |
| `sdks/cpp/include/rocketbox/port_probe.h` | List present USB ports (serial → silkscreen Port 1–4) |
| `sdks/cpp/src/` | SDK backends (USB via session/core; sim via TCP :1772) |
| `apps/wx/` | RocketBox App (desktop) — uses session directly |
| `apps/web/` | RocketBox App (PWA) — USB via `@rocketbox/sdk` |
| `sdks/typescript/` | `@rocketbox/sdk` |
| `rocketbox-tunnel/` | TUN ↔ RocketBox (uses C++ SDK only) |
| `simulated-hardware/` | Sole simulation device |
| `tools/` | `usb-probe`, `rocketbox-cli`, … |
| `99-rocketbox-usb.rules` | udev: MODE 0666 for 1772:0006 |
| `tests/` | CTest unit + integration + hardware |

## Layering rules

1. **USB logic** in `sdks/cpp/core/` only — no wx/GTK in core
2. **Session logic** in `sdks/cpp/session/` — no UI toolkit includes; orchestrator takes `RocketBoxTransport`
3. **Public C++ API** in `rocketbox/sdk.h` + `rocketbox/port_probe.h` + `rocketbox/session_orchestrator.h` — wx, tunnel, and CLI use this only. USB open uses **libusb sort index**; silkscreen Port 1–4 comes from cable serial (`list_present_ports` / `display_port()`).
4. **wx / tools** link `rocketbox_sdk` — do not construct `TransferController` in apps
5. **TypeScript USB/wire** in `sdks/typescript/` — PWA does not implement WebUSB
6. **One simulation product:** `simulated-hardware/`; in-process `sdks/cpp/sim` for CTest

## Testing

```bash
ctest --test-dir build -L unit
ctest --test-dir build -L integration
cd apps/web && npm test
```

## Conventions

- C++17, minimal diffs, match existing naming
- Dual-read for one release: `~/.config/rocketbox/` then `~/.config/sls-fabric/`; `ROCKETBOX_LOG` then `SLSFABRIC_LOG`; session inbox accepts `.rocketbox-session/` and `.fabric-session/`
- Update this file when major layout items complete
