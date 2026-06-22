# Architecture

## Design goal

One **USB transfer engine** (`fabric_usb_core`) shared by:

- RocketBox desktop (`apps/wx/` + `apps/demo/`)
- Web PWA (`apps/web/`)
- Headless test tools (`tools/`)

UI code never talks to libusb bulk endpoints directly; session apps use `TransferController` and the core API in `core/include/usb_transfer.h`.

## CMake graph

```
CMakeLists.txt
├── core/CMakeLists.txt          → fabric_usb_core (STATIC)
├── sim/CMakeLists.txt           → fabric_usb_sim (STATIC)
├── apps/demo/CMakeLists.txt     → sls_demo_logic (STATIC)
├── apps/wx/CMakeLists.txt       → RocketBox (EXECUTABLE)
└── tools/CMakeLists.txt         → usb-probe, usb-loopback-test, …
                                    PRIVATE: fabric_usb_core or sls_demo_logic
```

Rebuild from repo root:

```bash
cmake -S . -B build && cmake --build build -j
```

Binary locations:

- `build/core/libfabric_usb_core.a`
- `build/apps/wx/RocketBox`
- `build/tools/usb-probe`, `usb-loopback-test`

## Runtime: RocketBox wx

Implements the vendor contract in [GUI_HANDOFF.md](../GUI_HANDOFF.md) via shared session logic.

```
apps/wx/main.cpp
  └── MainFrame
        └── TransferOrchestrator + TransferController
              └── worker thread → send_file_core / receive_file_core
                    └── progress → wxTheApp->CallAfter → UI panels
```

| Handoff requirement | Implementation |
|---------------------|----------------|
| One `libusb_context*` for app lifetime | `TransferController` ctor/dtor |
| Send / receive on worker thread | `TransferController::start_worker` |
| One transfer at a time (per orchestrator rules) | `TransferOrchestrator` + UI state |
| `port_index` from `--port` CLI | `TransferController` |
| Progress ~every 4 MB, worker thread | core `progress_cb` → wx `CallAfter` |
| Show `error_message` verbatim on failure | status / error panels |
| Core functions silent (no stdout) | all I/O returns `TransferResult` only |

- **Main thread:** wx event loop, widgets, dialogs
- **Worker threads:** USB/file I/O via core + session listener thread

## Runtime: tools

| Tool | Links | Uses |
|------|-------|------|
| `usb-probe` | `fabric_usb_core` | libusb enumerate only |
| `usb-loopback-test` | `fabric_usb_core` | `loopback_transfer_core` |

Tools are the preferred way to validate core changes before exercising the GUI.

## Device model

- Multiple identical devices (`1772:0006`) may appear when several CON ports are cabled to one host
- **`port_index`** selects the Nth matching device (0-based) in libusb device list order
- **`count_fabric_devices(ctx)`** returns how many are connected

Loopback on one PC requires **two** devices (two cables). Run two RocketBox instances with `--port 0` and `--port 1`.

## Dependencies

| Component | Depends on |
|-----------|------------|
| `core` | libusb-1.0, pthread |
| `apps/demo` | core, sim (tests) |
| `apps/wx` | sls_demo_logic, wxWidgets |
| `tools` | core and/or sls_demo_logic |

## Adding a new app

1. Create `apps/<name>/CMakeLists.txt` with `add_executable(...)`
2. Link `sls_demo_logic` and/or `fabric_usb_core` as appropriate
3. `add_subdirectory(apps/<name>)` from root `CMakeLists.txt`
4. Document the new target in `README.md` and `AGENTS.md`

Do not duplicate USB logic in the new app — extend `core/` if the engine needs new capabilities.

## Adding a new tool

Add source under `tools/`, register in `tools/CMakeLists.txt`, link `fabric_usb_core` or `sls_demo_logic`.
