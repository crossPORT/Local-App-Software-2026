# data-transfer-demo

Linux desktop demo for USB file transfer over the SLS fabric FPGA device (`1772:0006`). The repo separates a **platform-neutral USB core** from **GTK UI** and **CLI test tools**.

## Quick start

### Prerequisites

- CMake ≥ 3.16, C++17 compiler
- `libusb-1.0` development package
- GTK 3 development package (`gtk+-3.0`)

On Ubuntu/Debian:

```bash
sudo apt install cmake g++ pkg-config libusb-1.0-0-dev libgtk-3-dev
```

### Build

```bash
cmake -S . -B build
cmake --build build -j
```

### USB permissions (Linux)

```bash
./scripts/setup-usb-access.sh
# Then unplug and replug the USB-C cable
```

This installs `99-sls-fabric-usb.rules` so libusb can open the device without root.

### Run

| Binary | Path after build | Purpose |
|--------|------------------|---------|
| RocketBox app | `build/apps/wx/data-transfer-demo` | Roster, send-to-peer, accept/reject, progress |
| Probe | `build/tools/usb-probe` | List matching devices and endpoints |
| Loopback test | `build/tools/usb-loopback-test <file>` | Two-cable same-PC loopback (ports 0→1) |

Typical bring-up sequence:

```bash
./build/tools/usb-probe
./build/apps/wx/data-transfer-demo --config ces-demo.conf
```

## Repository layout

```
├── CMakeLists.txt           # Top-level; adds core, apps, tools
├── core/                    # USB engine (static lib fabric_usb_core)
│   ├── include/             # usb_protocol.h, usb_transfer.h
│   └── src/                 # usb_transfer_core.cpp
├── apps/
│   └── gtk/                 # Linux GTK desktop app
├── tools/                   # Headless CLI utilities (link core only)
├── scripts/setup-usb-access.sh
├── 99-sls-fabric-usb.rules
└── Westcoast-0.01_release.zip   # Reference: proven Windows engine (see docs/)
```

## Architecture (short)

```
tools/*  ──┐
apps/wx  ──┼──► apps/demo (sls_demo_logic) ──► fabric_usb_core (core/) ──► libusb ──► FPGA 1772:0006
apps/demo ─┘
tools/*  ──┘
           └── wx only in apps/wx (not in core or tools)
```

- **`core/`** — All USB protocol and transfer logic. No GTK, no platform UI.
- **`apps/wx/`** — RocketBox Transfer UI (`MainFrame` + roster/send/incoming panels)
- **`apps/demo/`** — Shared session logic (`TransferOrchestrator`, `SessionListener`, `TransferController`)
- **`apps/gtk/`** — Legacy engineering demo (retained)
- **`tools/`** — Fast validation without launching the GUI.

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) and [docs/PROTOCOL.md](docs/PROTOCOL.md).

## Hardware expectations

- **VID/PID:** `0x1772` / `0x0006`
- **Endpoints:** EP2 OUT (`0x02`), EP1 IN (`0x81`)
- **Single cable:** send or receive on port index `0`
- **Loopback (same PC):** two USB cables to two CON ports; loopback uses port `0` (send) and port `1` (receive)

Port index = Nth matching device in libusb enumeration order (not necessarily physical silkscreen order).

## Reference: Westcoast 0.01

`Westcoast-0.01_release.zip` is the vendor reference archive. Its transfer engine is **ported into `core/`** (see [docs/WESTCOAST.md](docs/WESTCOAST.md)). The zip remains useful for diffing and for console-only diagnostic modes not yet extracted.

## Documentation index

| Document | Audience | Contents |
|----------|----------|----------|
| [README.md](README.md) | Everyone | Build, run, layout |
| [GUI_HANDOFF.md](GUI_HANDOFF.md) | GUI / app developers | Vendor contract: core API, threading, UX, errors |
| [AGENTS.md](AGENTS.md) | AI agents / new contributors | Repo map, conventions, open work |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Developers | Layers, threading, CMake targets |
| [docs/PROTOCOL.md](docs/PROTOCOL.md) | Core work | USB header, endpoints, current vs target |
| [docs/WESTCOAST.md](docs/WESTCOAST.md) | Engine porting | What to lift from the zip |

## Maintaining these docs

Update markdown when you change:

- CMake targets or directory layout → `README.md`, `AGENTS.md`, `docs/ARCHITECTURE.md`
- Vendor GUI API or UX contract → `GUI_HANDOFF.md` (+ `apps/gtk/` if implementing)
- `core/include/usb_protocol.h` or transfer semantics → `docs/PROTOCOL.md`
- Westcoast port progress → `docs/WESTCOAST.md` checklist
