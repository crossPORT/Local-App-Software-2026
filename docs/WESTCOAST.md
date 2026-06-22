# Westcoast 0.01 reference

Archive: `Westcoast-0.01_release.zip` (repo root).

This is the vendor **working** USB test tool (Windows, Visual Studio). The useful part is ~2144 lines in `Westcoast-0.01_release/main.cpp` — not the `.vs/` or build artifacts inside the zip.

## Extract for reading

```bash
unzip -p Westcoast-0.01_release.zip Westcoast-0.01_release/main.cpp | less
# or
unzip -p Westcoast-0.01_release.zip Westcoast-0.01_release/main.cpp > /tmp/westcoast-main.cpp
```

Do not commit extracted files or the zip’s IDE cache into git unless explicitly requested.

## What to port → `core/`

| Westcoast function | Port to | Notes |
|--------------------|---------|-------|
| `send_file_core` | `core/src/usb_transfer_core.cpp` | Async, 32-deep queue, 4 MiB padded frames |
| `receive_file_core` | same | Ring buffer + writer thread |
| `open_device_by_index` / `close_device` | same | Already similar; merge error handling |
| `bulk_write_all` / `bulk_read_all` | same | Sync helpers for headers |
| File header `ROCKETBX` | `core/include/usb_protocol.h` | Replaced Westcoast `BORNDIE` / legacy `FABR` headers |

## What NOT to port wholesale

- Windows includes (`windows.h`, `bcrypt.h`)
- Console menu `main()` and `do_*` stdin wrappers
- Modes 1–4, 7, 9–10 unless explicitly needed (diagnostic/stress/round-trip PC pairs)
- Monolithic file structure — keep modular `core/` + apps/tools

## GUI-relevant Westcoast modes

| Mode | Name | Maps to this repo |
|------|------|-------------------|
| 5 | Send file | `send_file_core` → GTK Send |
| 6 | Receive file | `receive_file_core` → GTK Receive |
| 8 | File cross-connect | Target for `loopback_transfer_core` |
| 11 | Crossbar toggle | **Removed** — fabric routes both directions natively |

Westcoast comments at top of `main.cpp` document the GUI handoff contract: core functions take parameters, return `TransferResult`, optional progress callback, no stdout/stdin.

That contract is captured for this repo in **[GUI_HANDOFF.md](../GUI_HANDOFF.md)**. The GTK app in `apps/gtk/` is the reference implementation. When porting the engine, preserve the handoff API in `core/include/usb_transfer.h` — do not break GUI integration.

## Key implementation details (from Westcoast)

### Send (`send_file_core`)

- `QUEUE_DEPTH = 32` async bulk OUT transfers
- Each submission is **full `CHUNK_SIZE` (4 MiB)** even for last partial block (zero-padded)
- `valid_bytes` tracks real data for progress; FPGA always sees full frames

### Receive (`receive_file_core`)

- `POOL_SIZE = QUEUE_DEPTH + 2` ring slots
- States: FREE → IN_FLIGHT → READY → writer consumes → FREE
- Fixes large-file corruption when disk writes lag USB (documented in Westcoast comments)

### Crossbar

- **Removed from this repo.** The fabric routes both directions natively, so the
  EP4 crossbar toggle was unused and could wedge the hardware. Not ported.

## Porting checklist

- [x] Replace header with 32-byte `ROCKETBX` in `usb_protocol.h` (was Westcoast `BORNDIE`)
- [x] Port async `send_file_core` with queue depth 32
- [x] Port ring-buffer `receive_file_core` with writer thread
- [x] Rework `loopback_transfer_core` to Westcoast mode 8 pipeline (`file_cross_connect_core`)
- [ ] Verify with `usb-loopback-test`, GTK app on hardware
- [x] Update [PROTOCOL.md](PROTOCOL.md) — engine matches Westcoast wire format

## Platform notes for port

Westcoast uses `libusb_handle_events_timeout` with `struct timeval` — available on Linux libusb. Replace any Windows-only APIs with portable C++17.

Keep `TransferResult` and `ProgressCallback` signatures in `usb_transfer.h` unchanged so `apps/gtk/` and `tools/` require minimal edits.

## Relationship to this repo

```
Westcoast main.cpp (reference)
        │
        ▼ port algorithms, not structure
core/src/usb_transfer_core.cpp
        │
        ├── apps/gtk/     (existing UI)
        └── tools/        (existing CLIs)
```
