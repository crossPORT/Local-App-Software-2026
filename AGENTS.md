# Agent onboarding

Read this first when working in this repository. Goal: USB file transfer to/from an FPGA fabric device (`1772:0006`) with RocketBox (wx + PWA) and CLI test tools.

## Read order

1. **[GUI_HANDOFF.md](GUI_HANDOFF.md)** — vendor GUI contract: the `*_core()` functions, threading, progress, errors, port index `0`
2. **This file** — repo layout, build, open work
3. **[protocols/session.md](protocols/session.md)** — session handshake best practices (no-buffer fabric, timing, listener rules)
4. **[protocols/file-transfer.md](protocols/file-transfer.md)** — ROCKETBX payload best practices (send/receive, mutex, staging)
5. **[docs/DEPLOYMENT.md](docs/DEPLOYMENT.md)** — CI, release installers, PWA zip on tags
6. **[docs/PROTOCOL.md](docs/PROTOCOL.md)** — wire format (if editing `core/`)
7. **[docs/WESTCOAST.md](docs/WESTCOAST.md)** — engine port from the zip

## 30-second context

- **Working reference:** `Westcoast-0.01_release.zip` (vendor release; Windows console tool with proven engine)
- **This repo:** CMake monorepo — `core/` engine + `lib/session/` shared logic + `apps/wx/` desktop + `apps/web/` PWA + `tools/` CLIs
- **Engine:** `core/src/usb_transfer_core.cpp` is ported from Westcoast (async send, ring-buffer receive, `ROCKETBX` header, mode-8 loopback). See [docs/WESTCOAST.md](docs/WESTCOAST.md).
- **Do not** start the GUI server for the user; they run binaries themselves
- **Do not** commit `Westcoast-0.01_release.zip` changes unless asked; treat it as read-only reference

## Build (always verify changes)

```bash
cmake -S . -B build
cmake --build build -j
```

All targets must compile:

- `fabric_usb_core`, `rocketbox_session` (static libs)
- `rocketbox` → **RocketBox** (wx desktop), `usb-probe`, `usb-loopback-test`

## File map

| Path | Role |
|------|------|
| `core/include/usb_protocol.h` | VID/PID, endpoints, header constants, chunk size |
| `core/include/usb_transfer.h` | Public core API: `TransferResult`, `*_core()` functions |
| `core/src/usb_transfer_core.cpp` | Engine implementation — **primary port target for Westcoast** |
| [GUI_HANDOFF.md](GUI_HANDOFF.md) | **Vendor GUI contract** — must-read for any UI work |
| `lib/session/` | Shared session layer (`transfer_orchestrator`, `session_listener`, `TransferController`, …) |
| `apps/wx/` | RocketBox desktop UI — roster, send-to-peer, accept/reject |
| `apps/web/` | RocketBox PWA (WebUSB) — TypeScript mirror of session behavior |
| `tools/usb_probe.cpp` | Enumerate devices/endpoints (no full transfer) |
| `tools/usb_loopback_test.cpp` | Two-port file loopback |
| `tools/booth_cli.cpp` | Headless session CLI (links `rocketbox_session`) |
| `scripts/setup-usb-access.sh` | Installs udev rule (needs sudo) |
| `99-sls-fabric-usb.rules` | udev: MODE 0666 for 1772:0006 |
| `tests/` | Automated suites: `unit-tests` (no hardware) + `hardware-tests` (needs cables) via CTest |

Extract Westcoast source for comparison (do not commit extracted tree):

```bash
unzip -p Westcoast-0.01_release.zip Westcoast-0.01_release/main.cpp > /tmp/westcoast-main.cpp
```

## Layering rules

1. **USB logic lives in `core/` only** — no wx/GTK in core, no `stdio` menu loops in core
2. **Session logic lives in `lib/session/`** — handshake, roster, orchestration; no UI toolkit includes
3. **UI lives in `apps/wx/`** — wxWidgets only; marshal worker/orchestrator updates via `wxTheApp->CallAfter`
4. **Web lives in `apps/web/`** — TypeScript; keep parity with `lib/session/` via golden fixtures + vitest
5. **Raw USB tools link `fabric_usb_core` only** — e.g. `usb-probe`, `usb-loopback-test`
6. **Session tools link `rocketbox_session`** — e.g. `booth-cli`, `fabric-session-test`
7. **New native apps** → `apps/<name>/` linking `rocketbox_session` and/or `fabric_usb_core`

## GUI integration (from GUI_HANDOFF.md)

The wx RocketBox app must follow the vendor handoff:

- **Two GUI operations:** send file, receive file — via `TransferController` → `*_core()` on a **worker thread**
- **`port_index`:** per app instance (`--port 0` / `--port 1` for two cables)
- **Progress:** callback on worker thread → `wxTheApp->CallAfter` before touching widgets
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
ctest --test-dir build -L unit          # pure logic, NO hardware — run in CI / before commits
ctest --test-dir build -L integration   # fabric_sim + TransferOrchestrator handshake (no USB)
ctest --test-dir build -L hardware      # real fabric transfers; auto-skips (code 77) if <2 devices
ctest --test-dir build                  # everything
```

- `unit-tests` — session message format, **golden session fixtures** (`tests/fixtures/session/`), meta/path-traversal safety, identity + session config parsing, peer roster, session role, core tuning (payload timeout, in-flight depth math/clamp, usbfs detection), **inbound receive partial-file cleanup** (`receive_payload`), fabric_sim transport. Add a unit test for any new pure logic.
- `integration-tests` — **full announce + offer/accept/ready + payload** over in-process `fabric_sim` with two `TransferOrchestrator` instances (ports 0↔1). Catches handshake timing, roster, auto-accept, and decline paths. Run after changing `transfer_orchestrator.cpp` or `session_listener.cpp`.
- `hardware-tests` — round-trip integrity across sizes incl. chunk boundaries and a payload that exceeds the usbfs pool (proves the in-flight auto-clamp).
- **PWA:** `cd apps/web && npm test` — vitest unit tests for session codec, handshake timing, config, roster, format helpers (golden fixtures shared with C++).
- Run a subset by substring: `./build/tests/unit-tests inflight` or `./build/tests/integration-tests handshake`

Manual hardware diagnostics (with cables):

```bash
./build/tools/usb-probe                    # expect 1+ devices when cable plugged in
./build/tools/usb-loopback-test /tmp/foo.bin   # needs 2 cables, ports 0 and 1
```

Permission errors → run `./scripts/setup-usb-access.sh`, replug cable.

## Priority open work

1. **Hardware validation** — run `usb-probe`, `usb-loopback-test`, and RocketBox wx against real FPGA
2. Optional: `apps/console/` — thin Linux menu wrapper for Westcoast diagnostic modes 1–4, 7, 9–10
3. Optional: cancel support for in-flight transfers (not in vendor handoff yet)

## Conventions

- C++17, minimal diffs, match existing naming (`snake_case` functions, `TransferResult`, `usb_protocol::` constants)
- Comments only for non-obvious protocol/hardware behavior
- Update [docs/PROTOCOL.md](docs/PROTOCOL.md) when changing wire format
- Update [GUI_HANDOFF.md](GUI_HANDOFF.md) only if the vendor API contract changes (coordinate with FPGA team)
- Update this file’s “Priority open work” when major items complete

## Common mistakes to avoid

See also **[protocols/session.md](protocols/session.md)** and **[protocols/file-transfer.md](protocols/file-transfer.md)** for handshake timing and payload sequencing.

- Putting GUI toolkit includes in `core/`
- Changing header magic/endpoints without checking Westcoast reference and FPGA expectations
- Using sync bulk transfers for large file payload after Westcoast port (regresses throughput and large-file correctness)
- Assuming port index equals physical port label — always use libusb enumeration order
