# Agent onboarding

Read this first when working in this repository. Goal: USB file transfer to/from an FPGA fabric device (`1772:0006`) on Linux, with a GTK app and CLI test tools.

## Read order

1. **[GUI_HANDOFF.md](GUI_HANDOFF.md)** — vendor GUI contract: the `*_core()` functions, threading, progress, errors, port index `0`
2. **This file** — repo layout, build, open work
3. **[docs/PROTOCOL.md](docs/PROTOCOL.md)** — wire format (if editing `core/`)
4. **[docs/WESTCOAST.md](docs/WESTCOAST.md)** — engine port from the zip

## 30-second context

- **Working reference:** `Westcoast-0.01_release.zip` (vendor release; Windows console tool with proven engine)
- **This repo:** CMake project with `core/` engine + `apps/wx/` RocketBox UI + `apps/demo/` session logic + `tools/` CLIs
- **Engine:** `core/src/usb_transfer_core.cpp` is ported from Westcoast (async send, ring-buffer receive, `BORNDIE` header, mode-8 loopback). See [docs/WESTCOAST.md](docs/WESTCOAST.md).
- **Do not** start the GUI server for the user; they run binaries themselves
- **Do not** commit `Westcoast-0.01_release.zip` changes unless asked; treat it as read-only reference

## Build (always verify changes)

```bash
cmake -S . -B build
cmake --build build -j
```

All targets must compile:

- `fabric_usb_core` (static lib)
- `data-transfer-demo`, `usb-probe`, `usb-loopback-test`

## File map

| Path | Role |
|------|------|
| `core/include/usb_protocol.h` | VID/PID, endpoints, header constants, chunk size |
| `core/include/usb_transfer.h` | Public core API: `TransferResult`, `*_core()` functions |
| `core/src/usb_transfer_core.cpp` | Engine implementation — **primary port target for Westcoast** |
| [GUI_HANDOFF.md](GUI_HANDOFF.md) | **Vendor GUI contract** — must-read for any UI work |
| RocketBox wx app | `apps/wx/main_frame.{h,cpp}` + panels | Roster, send-to-peer, accept/reject (RocketBox MVP) |
| Shared demo logic | `apps/demo/` (`transfer_orchestrator`, `session_listener`, …) | USB session coordination + `TransferController` |
| Legacy GTK app | `apps/gtk/` | Engineering demo (retained; not RocketBox MVP target) |
| `apps/gtk/transfer_controller.{h,cpp}` | Legacy duplicate of `apps/demo/transfer_controller` |
| `tools/usb_probe.cpp` | Enumerate devices/endpoints (no full transfer) |
| `tools/usb_loopback_test.cpp` | Two-port file loopback |
| `scripts/setup-usb-access.sh` | Installs udev rule (needs sudo) |
| `99-sls-fabric-usb.rules` | udev: MODE 0666 for 1772:0006 |
| `tests/` | Automated suites: `unit-tests` (no hardware) + `hardware-tests` (needs cables) via CTest |

Extract Westcoast source for comparison (do not commit extracted tree):

```bash
unzip -p Westcoast-0.01_release.zip Westcoast-0.01_release/main.cpp > /tmp/westcoast-main.cpp
```

## Layering rules

1. **USB logic lives in `core/` only** — no GTK, no `stdio` menu loops in core
2. **UI lives in `apps/wx/`** (RocketBox MVP) — wxWidgets; marshal worker/orchestrator updates via `wxTheApp->CallAfter`
3. **Tools link `fabric_usb_core` only** — no GTK dependency
4. **New platforms** → new `apps/<platform>/` subdirectory linking core (e.g. future `apps/console/`, `apps/win32/`)

## GUI integration (from GUI_HANDOFF.md)

The GTK app must follow the vendor handoff:

- **Two GUI operations:** send file, receive file — all via `*_core()` on a **worker thread**
- **`port_index`:** always `0` for those (single cable to PC)
- **Progress:** callback fires ~every 4 MB on the worker thread → marshal to GTK main thread before touching widgets (`g_idle_add` in `MainWindow::schedule_ui_update`)
- **Receive UX:** show `"Waiting for sender..."` until the first progress callback (header wait, up to 120s)
- **Errors:** display `TransferResult.error_message` verbatim; core does not throw
- **No cancel** — one transfer at a time until complete or fail
- **libusb:** one `libusb_context*` for app lifetime; `libusb_init` in `TransferController` ctor, `libusb_exit` in dtor

Repo-only GUI extras: loopback button (ports 0→1), device-count hint — see handoff “Repo extensions” section.

Full detail: [GUI_HANDOFF.md](GUI_HANDOFF.md).

## Public core API

Declared in `core/include/usb_transfer.h` (authoritative; handoff mirrors this):

```cpp
TransferResult send_file_core(ctx, path, port_index, progress_cb);
TransferResult receive_file_core(ctx, out_path, port_index, progress_cb);
TransferResult loopback_transfer_core(ctx, path, send_port, recv_port, progress_cb);
int count_fabric_devices(ctx);
```

`TransferResult` carries `ok`, byte counts, timing, `mbps`, and `error_message`. GUI and tools consume this struct directly — do not parse log strings.

## Testing

Automated suites live in `tests/` and run via CTest (dependency-free framework, no GoogleTest). Keep the **core/session logic covered here** so GUI work stays stable.

```bash
ctest --test-dir build -L unit       # pure logic, NO hardware — run in CI / before commits
ctest --test-dir build -L hardware   # real fabric transfers; auto-skips (code 77) if <2 devices
ctest --test-dir build               # everything
```

- `unit-tests` — session message format, meta/path-traversal safety, identity + demo config parsing, peer roster, session role, core tuning (payload timeout, in-flight depth math/clamp, usbfs detection), **inbound receive partial-file cleanup** (`receive_payload`). Add a unit test for any new pure logic.
- `hardware-tests` — round-trip integrity across sizes incl. chunk boundaries and a payload that exceeds the usbfs pool (proves the in-flight auto-clamp).
- Run a subset by substring: `./build/tests/unit-tests inflight`.

Manual hardware diagnostics (with cables):

```bash
./build/tools/usb-probe                    # expect 1+ devices when cable plugged in
./build/tools/usb-loopback-test /tmp/foo.bin   # needs 2 cables, ports 0 and 1
```

Permission errors → run `./scripts/setup-usb-access.sh`, replug cable.

## Priority open work

1. **Hardware validation** — run `usb-probe`, `usb-loopback-test`, and GTK app against real FPGA
2. Optional: `apps/console/` — thin Linux menu wrapper for Westcoast diagnostic modes 1–4, 7, 9–10
3. Optional: cancel support for in-flight transfers (not in vendor handoff yet)

## Conventions

- C++17, minimal diffs, match existing naming (`snake_case` functions, `TransferResult`, `usb_protocol::` constants)
- Comments only for non-obvious protocol/hardware behavior
- Update [docs/PROTOCOL.md](docs/PROTOCOL.md) when changing wire format
- Update [GUI_HANDOFF.md](GUI_HANDOFF.md) only if the vendor API contract changes (coordinate with FPGA team)
- Update this file’s “Priority open work” when major items complete

## Common mistakes to avoid

- Putting GTK includes in `core/`
- Changing header magic/endpoints without checking Westcoast reference and FPGA expectations
- Using sync bulk transfers for large file payload after Westcoast port (regresses throughput and large-file correctness)
- Assuming port index equals physical port label — always use libusb enumeration order
