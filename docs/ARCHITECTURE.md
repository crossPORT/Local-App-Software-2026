# Architecture

## Monorepo layout

| Path | Kind | Role |
|------|------|------|
| `core/` | Library | USB engine (`fabric_usb_core`) — libusb, ROCKETBX wire protocol |
| `lib/session/` | Library | Shared native session logic (`rocketbox_session`) — handshake, roster, orchestration |
| `apps/wx/` | App | RocketBox desktop UI (wxWidgets) — links `rocketbox_session` |
| `apps/web/` | App | RocketBox PWA (TypeScript/WebUSB) — **parallel implementation**, does not link C++ |
| `sim/` | Library | In-process fabric simulator for tests |
| `tools/` | Binaries | CLIs — raw USB tools link `core`; session tools link `rocketbox_session` |
| `tests/` | Binaries | C++ unit + integration suites (CTest) |

**Rule:** `apps/` holds runnable products only. Shared C++ logic lives under `lib/`.

## Design goal

One **USB transfer engine** (`fabric_usb_core`) and one **session layer** (`rocketbox_session`) shared by:

- RocketBox desktop (`apps/wx/` + `lib/session/`)
- Headless session tools (`booth-cli`, `fabric-session-test`, …)

The web PWA (`apps/web/`) mirrors session behavior in TypeScript and stays aligned via golden fixtures and parity tests — it does not link the C++ libraries.

UI code never talks to libusb bulk endpoints directly; session apps use `TransferController` and the core API in `core/include/usb_transfer.h`.

## CMake graph

```
CMakeLists.txt
├── core/CMakeLists.txt              → fabric_usb_core (STATIC)
├── sim/CMakeLists.txt               → fabric_usb_sim (STATIC)
├── lib/session/CMakeLists.txt       → rocketbox_session (STATIC)
├── apps/wx/CMakeLists.txt           → rocketbox → RocketBox (EXECUTABLE)
└── tools/CMakeLists.txt             → usb-probe, booth-cli, …
```

Web app: `cd apps/web && npm ci && npm run build` (separate from root CMake).

Rebuild native stack from repo root:

```bash
cmake -S . -B build && cmake --build build -j
```

Binary locations:

- `build/core/libfabric_usb_core.a`
- `build/lib/session/librocketbox_session.a`
- `build/apps/wx/RocketBox`
- `build/tools/usb-probe`, `usb-loopback-test`, `booth-cli`, …

## Runtime: RocketBox wx

Implements the core API via shared session logic in `lib/session/`.

```
apps/wx/main.cpp
  └── MainFrame
        └── TransferOrchestrator + TransferController  (lib/session/)
              └── worker thread → send_file_core / receive_file_core  (core/)
                    └── progress → wxTheApp->CallAfter → UI panels
```

| Handoff requirement | Implementation |
|---------------------|----------------|
| One `libusb_context*` for app lifetime | `TransferController` ctor/dtor |
| Send / receive on worker thread | `TransferController::start_worker` |
| One transfer at a time (per orchestrator rules) | `TransferOrchestrator` + UI state |
| `port_index` from `--port` CLI or cable picker (`fabric_device_picker.cpp`) | `MainFrame` → `TransferOrchestrator` |
| Progress ~every 4 MB, worker thread | core `progress_cb` → wx `CallAfter` |
| Show `error_message` verbatim on failure | status / error panels |
| Core functions silent (no stdout) | all I/O returns `TransferResult` only |

- **Main thread:** wx event loop, widgets, dialogs
- **Worker threads:** USB/file I/O via core + session listener thread

## Runtime: web PWA

TypeScript orchestrator and handshake in `apps/web/src/lib/`. Same user-facing behavior as wx; validated with `npm test` (vitest) against shared golden session fixtures in `tests/fixtures/session/`.

## Runtime: tools

| Tool | Links | Uses |
|------|-------|------|
| `usb-probe` | `fabric_usb_core` | libusb enumerate only |
| `usb-loopback-test` | `fabric_usb_core` | `loopback_transfer_core` |
| `usb-pair-test` | `fabric_usb_core` | pair diagnostics |
| `fabric-session-test` | `rocketbox_session` | full session over USB or sim |
| `booth-cli` | `rocketbox_session` | headless booth/session CLI |
| `send-repro` | `rocketbox_session` | headless send-path repro |

Prefer `usb-loopback-test` and integration tests to validate core/session changes before exercising the GUI.

## Device model

- Multiple identical devices (`1772:0006`) may appear when several CON ports are cabled to one host
- **`port_index`** selects the Nth matching device (0-based) in libusb device list order
- **`count_fabric_devices(ctx)`** returns how many are connected

Loopback on one PC requires **two** devices (two cables). Run two RocketBox instances. Use `--port 0` and `--port 1`, or pick the cable in the **Connect USB** dialog when multiple devices are present.

## Dependencies

| Component | Depends on |
|-----------|------------|
| `core` | libusb-1.0, pthread |
| `lib/session` | core, sim |
| `apps/wx` | rocketbox_session, wxWidgets |
| `apps/web` | npm toolchain only (runtime: browser WebUSB) |
| `tools` | core and/or rocketbox_session |

## Adding a new native app

1. Create `apps/<name>/CMakeLists.txt` with `add_executable(...)`
2. Link `rocketbox_session` and/or `fabric_usb_core` as appropriate
3. `add_subdirectory(apps/<name>)` from root `CMakeLists.txt`
4. Document the new target in `README.md` and `AGENTS.md`

Do not duplicate USB or session logic in the new app — extend `core/` or `lib/session/` instead.

## Adding a new tool

Add source under `tools/`, register in `tools/CMakeLists.txt`, link `fabric_usb_core` or `rocketbox_session`.
