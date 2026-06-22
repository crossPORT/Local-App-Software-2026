# USB Test Tool — GUI Handoff Notes

**Vendor contract** for integrating the USB core into a desktop UI. The
`*_core()` functions and behavior below are what the FPGA team expects a GUI
to use. Do not change their signatures or error semantics without coordination.

**In this repo**

| Handoff topic | Where it lives |
|---------------|----------------|
| Core API (`TransferResult`, `*_core()`) | `core/include/usb_transfer.h` — **source of truth** if this doc and headers differ |
| RocketBox wx implementation | `apps/wx/` + `apps/demo/transfer_controller.{h,cpp}` |
| Engine (being aligned with Westcoast) | `core/src/usb_transfer_core.cpp` |
| Agent / build context | [AGENTS.md](AGENTS.md), [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) |

**Repo extensions beyond this handoff** (tools and GUI extras, not part of the
original send/receive contract):

- `loopback_transfer_core()` — two-cable same-PC test (GUI button + `usb-loopback-test`)
- `count_fabric_devices()` — device-count hint in the wx status UI

For GUI send / receive, follow this document. Use port index `0` only
unless running **two GUI instances** on one PC (see below).

---

## Two GUI instances (same PC, two cables)

To mimic two distinct machines, launch one app per port:

```bash
./build/apps/wx/RocketBox --port 0 --config booth-port0.conf
./build/apps/wx/RocketBox --port 1 --config booth-port1.conf
```

Short form: `-p 0` / `-p 1`.

**Workflow**

1. On **port 0** window: wait for peer / accept incoming transfer
2. On **port 1** window: send to peer from roster
3. Watch progress on both windows; compare received file to source

Each instance owns one USB device; send/receive use that instance's `--port`.
Loopback (ports 0→1 in one process) stays on the **port 0** window only.

---

## What the tool does

Talks to an FPGA over USB (VID 0x1772, PID 0x0006) via libusb. Two
operations matter for the GUI:

- **Send file**: stream a file from PC to FPGA
- **Receive file**: stream a file from FPGA to PC

Send and receive are long-running on big files (20 GB+ is normal),
so run them on a worker thread.

## The functions you call

```cpp
struct TransferResult {
    bool        ok;                 // true on success
    uint64_t    bytes_transferred;  // actual bytes moved
    uint64_t    expected_bytes;     // expected total
    double      seconds;            // wall-clock duration
    double      mbps;               // throughput (MiB/s)
    std::string error_message;      // populated iff ok == false
};

using ProgressCallback = std::function<void(uint64_t done,
                                            uint64_t total,
                                            double   elapsed_secs)>;

TransferResult send_file_core   (libusb_context* ctx,
                                 const std::string& path,
                                 int port_index,
                                 ProgressCallback progress_cb = nullptr);

TransferResult receive_file_core(libusb_context* ctx,
                                 const std::string& out_path,
                                 int port_index,
                                 ProgressCallback progress_cb = nullptr);
```

These are silent. No prints, no prompts. All status comes back in
`TransferResult`. Errors do not throw; check `result.ok` and show
`result.error_message` if false.

## port_index

Always pass `0`. There's only one cable to the PC. The parameter
exists for the two-cable case but we're not using it.

## Setup and teardown

Once at app startup:
```cpp
libusb_context* ctx = nullptr;
libusb_init(&ctx);
```

Once at app shutdown:
```cpp
libusb_exit(ctx);
```

Pass the same `ctx` to every core call.

## Progress callback

Fires roughly once per 4 MB chunk completed. Fires on the worker
thread, so marshal to the GUI thread before touching widgets.

## Threading

Only one transfer at a time. Worker thread calls the core function,
result comes back when it's done.

## Error strings

The `error_message` field is meant to be displayed verbatim. Real cases:

- `"Could not open USB device at port index 0"`: cable unplugged
  or FPGA not enumerated
- `"Could not open file: ..."` / `"Could not create output file: ..."`:
  bad path or permissions
- `"Header send failed"` / `"Header read failed"` / `"Bad magic bytes in header"`:
  other side isn't running. For receive, means no sender started
  within the 120s header timeout.
- `"USB transfer failed, status=N"`: cable yanked, FPGA hung, or
  timeout
- `"Disk write error"`: out of space or drive disconnected

## Two things to know

- **Receive blocks waiting for a sender.** When the user clicks
  Receive, the call sits waiting for the header before any progress
  callback fires. Show "Waiting for sender..." until first progress.
  (`TransferController` sets `waiting_for_sender` and `MainWindow` displays this.)
- **No cancel support.** Once a transfer starts, it runs to
  completion or failure. If you need cancel, ask and I'll add it.

## Linux note (this repo)

On Linux, libusb may return a permission error before the device opens. The
core may surface:

- `"USB permission denied. Run once: ./scripts/setup-usb-access.sh (then unplug/replug the cable)"`

Run [scripts/setup-usb-access.sh](scripts/setup-usb-access.sh) once, then replug
the cable. See [README.md](README.md).

## Related docs

- [docs/PROTOCOL.md](docs/PROTOCOL.md) — wire format (header/endpoints; engine port in progress)
- [docs/WESTCOAST.md](docs/WESTCOAST.md) — proven reference engine in `Westcoast-0.01_release.zip`
- [AGENTS.md](AGENTS.md) — agent onboarding and file map
