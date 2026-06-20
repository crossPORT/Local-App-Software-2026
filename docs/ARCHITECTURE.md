# Architecture

## Design goal

One **USB transfer engine** (`fabric_usb_core`) shared by:

- Desktop GUI (`apps/gtk/`)
- Headless test tools (`tools/`)
- Future platform apps (console, Windows, etc.)

UI code never talks to libusb bulk endpoints directly; it goes through `core/include/usb_transfer.h`.

## CMake graph

```
CMakeLists.txt
├── core/CMakeLists.txt          → fabric_usb_core (STATIC)
│                                   PUBLIC: libusb, Threads, core/include
├── apps/gtk/CMakeLists.txt      → data-transfer-demo (EXECUTABLE)
│                                   PRIVATE: fabric_usb_core, gtk+-3.0
└── tools/CMakeLists.txt         → usb-probe, usb-loopback-test
                                    PRIVATE: fabric_usb_core
```

Rebuild from repo root:

```bash
cmake -S . -B build && cmake --build build -j
```

Binary locations:

- `build/core/libfabric_usb_core.a`
- `build/apps/gtk/data-transfer-demo`
- `build/tools/usb-probe`, `usb-loopback-test`

## Runtime: GTK app

Implements the vendor contract in [GUI_HANDOFF.md](../GUI_HANDOFF.md).

```
main.cpp
  └── MainWindow
        └── TransferController (owns libusb_context*)
              └── std::thread worker
                    └── send_file_core / receive_file_core
                          └── progress_cb → g_idle_add → UI update
```

| Handoff requirement | Implementation |
|---------------------|----------------|
| One `libusb_context*` for app lifetime | `TransferController` ctor/dtor |
| Send / receive on worker thread | `TransferController::start_worker` |
| Only one transfer at a time | `state_.busy` guard + disabled buttons |
| `port_index = 0` for the core ops | `kPortIndex` in `transfer_controller.cpp` |
| Progress ~every 4 MB, worker thread | core `progress_cb` → `TransferController` → `MainWindow::schedule_ui_update` → `g_idle_add` |
| Receive: "Waiting for sender..." until first progress | `waiting_for_sender` in `TransferUiState` |
| Show `error_message` verbatim on failure | `MainWindow` error label |
| No cancel | not implemented (by design) |
| Core functions silent (no stdout) | all I/O in `core/` returns `TransferResult` only |

- **Main thread:** GTK event loop, widgets, file choosers
- **Worker thread:** blocking USB/file I/O via core functions
- **Repo extension:** loopback button calls `loopback_transfer_core(..., 0, 1, ...)` — not in the original send/receive handoff

## Runtime: tools

| Tool | Links core | Uses |
|------|------------|------|
| `usb-probe` | yes (protocol constants) | libusb enumerate only |
| `usb-loopback-test` | yes | `loopback_transfer_core` |

Tools are the preferred way to validate core changes before exercising the GUI.

## Device model

- Multiple identical devices (`1772:0006`) may appear when several CON ports are cabled to one host
- **`port_index`** selects the Nth matching device (0-based) in libusb device list order
- **`count_fabric_devices(ctx)`** returns how many are connected

Loopback on one PC requires **two** devices (two cables). The GUI shows a hint based on device count.

## Dependencies

| Component | Depends on |
|-----------|------------|
| `core` | libusb-1.0, pthread |
| `apps/gtk` | core, GTK 3 |
| `tools` | core |

## Adding a new app

1. Create `apps/<name>/CMakeLists.txt` with `add_executable(...)`
2. `target_link_libraries(... PRIVATE fabric_usb_core ...)`
3. `add_subdirectory(apps/<name>)` from root `CMakeLists.txt`
4. Document the new target in `README.md` and `AGENTS.md`

Do not duplicate USB logic in the new app — extend `core/` if the engine needs new capabilities.

## Adding a new tool

Add source under `tools/`, register in `tools/CMakeLists.txt`, link `fabric_usb_core`.
