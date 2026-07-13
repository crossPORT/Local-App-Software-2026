# Architecture

## Monorepo layout

| Path | Kind | Role |
|------|------|------|
| `sdks/cpp/core/` | Library | USB engine (`rocketbox_usb_core`) — libusb, ROCKETBX wire protocol |
| `sdks/cpp/session/` | Library | Shared native session logic (`rocketbox_session`) — handshake, roster, orchestration |
| `sdks/cpp/include/rocketbox/sdk.h` | SDK | Public C++ API — `create_rocketbox_transport` (USB or sim); no Attach |
| `apps/wx/` | App | RocketBox App (desktop / wxWidgets) — links `rocketbox_session` (**real USB**) |
| `apps/web/` | App | RocketBox App (PWA / WebUSB) — UI; USB via `@rocketbox/sdk`; `?simulate=1` or real WebUSB |
| `simulated-hardware/` | Service | Sole sim product — TCP **1772** (C++), WS **1773** (PWA) + dashboard |
| `sdks/typescript/` | SDK | `@rocketbox/sdk` — `createRocketBoxTransport` |
| `rocketbox-tunnel/` | App | RocketBox ↔ host IP; `--transport sim` or `usb` |
| `sdks/cpp/sim/` | Library | In-process simulator (`rocketbox_usb_sim`) for native tests |
| `tools/` | Binaries | CLIs — raw USB tools link core; session tools link `rocketbox_session` |
| `tests/` | Binaries | C++ unit + integration suites (CTest) |

**Rule:** `apps/` holds runnable products only. Shared C++ logic lives under `sdks/cpp/`.

For clone → run/test steps (sim **and** hardware), see **[DEV-DEMO.md](DEV-DEMO.md)**.

## Design goal

One **USB transfer engine** (`rocketbox_usb_core`) and one **session layer** (`rocketbox_session`) shared by:

- RocketBox App desktop (`apps/wx/` + `sdks/cpp/session/`)
- Headless session tools (`rocketbox-cli`, `session-test`, …)

The web PWA (`apps/web/`) mirrors session behavior in TypeScript via `@rocketbox/sdk` and stays aligned via golden fixtures and parity tests — it does not link the C++ libraries.

UI code never talks to libusb bulk endpoints directly; session apps use `TransferController` and the core API in `sdks/cpp/core/include/usb_transfer.h`.

## CMake graph

```
CMakeLists.txt
├── sdks/cpp/core/CMakeLists.txt     → rocketbox_usb_core (STATIC)
├── sdks/cpp/sim/CMakeLists.txt      → rocketbox_usb_sim (STATIC)
├── sdks/cpp/session/CMakeLists.txt  → rocketbox_session (STATIC)
├── sdks/cpp/CMakeLists.txt          → rocketbox_sdk (STATIC)
├── rocketbox-tunnel/CMakeLists.txt  → rocketbox-tunnel (EXECUTABLE)
├── apps/wx/CMakeLists.txt           → rocketbox → RocketBox (EXECUTABLE)
└── tools/CMakeLists.txt             → usb-probe, rocketbox-cli, session-test, …
```

Web app: `cd apps/web && npm ci && npm run build` (separate from root CMake).

Rebuild native stack from repo root:

```bash
cmake -S . -B build && cmake --build build -j
```

Binary locations:

- `build/sdks/cpp/core/librocketbox_usb_core.a`
- `build/sdks/cpp/session/librocketbox_session.a`
- `build/apps/wx/RocketBox`
- `build/rocketbox-tunnel/rocketbox-tunnel`
- `build/tools/usb-probe`, `usb-loopback-test`, `rocketbox-cli`, `session-test`, …

## Runtime: RocketBox App (wx)

Implements the core API via shared session logic in `sdks/cpp/session/`.

```
apps/wx/main.cpp
  └── MainFrame
        └── TransferOrchestrator + TransferController  (sdks/cpp/session/)
              └── worker thread → send_file_core / receive_file_core  (sdks/cpp/core/)
                    └── progress → wxTheApp->CallAfter → UI panels
```

| Handoff requirement | Implementation |
|---------------------|----------------|
| One `libusb_context*` for app lifetime | `TransferController` ctor/dtor |
| Send / receive on worker thread | `TransferController::start_worker` |
| One transfer at a time (per orchestrator rules) | `TransferOrchestrator` + UI state |
| `port_index` from `--port` CLI or cable picker (`rocketbox_device_picker.cpp`) | `MainFrame` → `TransferOrchestrator` |
| Progress ~every 4 MB, worker thread | core `progress_cb` → wx `CallAfter` |
| Show `error_message` verbatim on failure | status / error panels |
| Core functions silent (no stdout) | all I/O returns `TransferResult` only |

- **Main thread:** wx event loop, widgets, dialogs
- **Worker threads:** USB/file I/O via core + session listener thread

## Runtime: web PWA

TypeScript orchestrator and handshake in `apps/web/src/lib/`. USB/wire via `@rocketbox/sdk` (`createRocketBoxTransport`). Same user-facing behavior as wx; validated with `npm test` (vitest) against shared golden session fixtures in `tests/fixtures/session/`.

## Runtime: tools

| Tool | Links | Uses |
|------|-------|------|
| `usb-probe` | `rocketbox_usb_core` | libusb enumerate only |
| `usb-loopback-test` | `rocketbox_usb_core` | `loopback_transfer_core` |
| `usb-pair-test` | `rocketbox_usb_core` | pair diagnostics |
| `session-test` | `rocketbox_session` | full session over USB or sim |
| `rocketbox-cli` | `rocketbox_session` | headless session CLI |
| `send-repro` | `rocketbox_session` | headless send-path repro |

Prefer `usb-loopback-test` and integration tests to validate core/session changes before exercising the GUI.

## Device model

- Multiple identical devices (`1772:0006`) may appear when several CON ports are cabled to one host
- **`port_index`** selects the Nth matching device (0-based) in libusb device list order
- **`count_rocketbox_devices(ctx)`** returns how many are connected

Loopback on one PC requires **two** RocketBox hardware connections (two cables). Run two RocketBox App instances. Use `--port 0` and `--port 1`, or pick the cable in the **Connect USB** dialog when multiple devices are present.

## Dependencies

| Component | Depends on |
|-----------|------------|
| `sdks/cpp/core` | libusb-1.0, pthread |
| `sdks/cpp/session` | core, sim |
| `apps/wx` | rocketbox_session, wxWidgets |
| `apps/web` | npm toolchain only (runtime: browser WebUSB via SDK) |
| `rocketbox-tunnel` | rocketbox_sdk |
| `tools` | core and/or rocketbox_session |

## Adding a new native app

1. Create `apps/<name>/CMakeLists.txt` with `add_executable(...)`
2. Link `rocketbox_session` and/or `rocketbox_usb_core` as appropriate
3. `add_subdirectory(apps/<name>)` from root `CMakeLists.txt`
4. Document the new target in `README.md` and `AGENTS.md`

Do not duplicate USB or session logic in the new app — extend `sdks/cpp/core/` or `sdks/cpp/session/` instead.

## Adding a new tool

Add source under `tools/`, register in `tools/CMakeLists.txt`, link `rocketbox_usb_core` or `rocketbox_session`.
