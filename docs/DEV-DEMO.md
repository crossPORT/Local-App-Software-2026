# Developer demo: simulation + real hardware

One place to clone the repo, understand the main sections, and run/test against **simulated hardware** or a **real RocketBox** (`1772:0006`).

## Repo map

| Path | What it is | Sim | Real USB |
|------|------------|-----|----------|
| `simulated-hardware/` | Crossport stand-in (TCP **1772** + WebSocket **1773** + dashboard) | Yes | — |
| `apps/web/` | RocketBox App PWA (UI; USB via SDK) | `?simulate=1` → SDK | SDK WebUSB |
| `apps/wx/` | RocketBox App desktop (wxWidgets) | No | libusb |
| `sdks/typescript/` | TS SDK: `createRocketBoxTransport`, ROCKETBX + session codecs | WS 1773 | WebUSB |
| `sdks/cpp/` | C++ Session/Connection + sim/USB transports | TCP 1772 | libusb |
| `fabric-tunnel/` | Fabric ↔ host IP bridge | `--transport sim` | `--transport usb` |
| `core/` + `lib/session/` | Native USB engine + session orchestration (wx / CLIs) | — | Yes |
| `tools/` | `usb-probe`, `booth-cli`, loopback, … | — | Mostly USB |
| `tests/` | CTest unit + integration; web vitest | Some | Hardware labels |

**Rule of thumb:** PWA captures `?simulate=` / `?port=` and calls `@rocketbox/sdk` `createRocketBoxTransport` — all WebUSB/ROCKETBX/sim I/O is in the SDK. The **wx desktop app talks to real USB only** today.

---

## Prerequisites

- **Node.js 18+** (sim daemon + PWA)
- **CMake**, **C++17**, **libusb**, **wxWidgets 3.2+** (native desktop)
- **Chrome or Edge** for the PWA
- **Docker** optional (sim via Compose)

Linux packages (Ubuntu/Debian example):

```bash
sudo apt install cmake g++ pkg-config libusb-1.0-0-dev libwxgtk3.2-dev
```

Clone and enter the repo:

```bash
git clone <repo-url> Local-App-Software-2026
cd Local-App-Software-2026
```

---

## 1. Start simulated hardware

From a terminal (leave it running):

```bash
cd simulated-hardware
npm install
npm start
# or: docker compose up --build
```

| Port | Clients |
|------|---------|
| **1772/tcp** | C++ SDK, fabric-tunnel (`--transport sim`) |
| **1773** | PWA WebSocket sim + live dashboard |

Dashboard: [http://localhost:1773/](http://localhost:1773/)

Details: [simulated-hardware/README.md](../simulated-hardware/README.md).

---

## 2. PWA against the simulator

```bash
cd apps/web
npm ci
npm run dev
```

Open **two** browser windows (or profiles), one per fabric port:

| Window | URL |
|--------|-----|
| Port 1 | `https://localhost:5173/?simulate=1&port=1` |
| Port 2 | `https://localhost:5173/?simulate=1&port=2` |

(Vite may use another port; keep `?simulate=1&port=N`. HTTPS comes from the Vite SSL plugin.)

1. Set a **display name** in Settings on each window.
2. Click **Connect USB** (sim path — no real cable).
3. Confirm both peers appear; drag a file to send.
4. Watch control/data activity on the sim dashboard at `:1773`.

Query flags:

| Param | Meaning |
|-------|---------|
| `simulate=1` | PWA passes `simulate` to SDK → `SimTransport` → `ws://localhost:1773?port=N` |
| `port=1..4` | Fabric port (default `1`); PWA passes through to SDK |

---

## 3. PWA against real RocketBox hardware

1. Plug in the RocketBox cable.
2. **Linux once:** `./scripts/setup-usb-access.sh` then unplug/replug.
3. Start the PWA **without** simulate:

```bash
cd apps/web && npm ci && npm run dev
# open https://localhost:5173/   (no ?simulate=)
```

4. **Connect USB** → browser WebUSB picker (handled by `@rocketbox/sdk`, not PWA-local USB code).
5. Second laptop/cable = second peer (or a second port if the fabric exposes multiple).

WebUSB requires **HTTPS** (or localhost). `file://` will not work. The PWA must not implement WebUSB itself — see [apps/web/AGENTS.md](../apps/web/AGENTS.md).

---

## 4. Native desktop (wx) — real hardware

```bash
cmake -S . -B build
cmake --build build -j
./scripts/setup-usb-access.sh   # Linux once; replug cable
./build/apps/wx/RocketBox
```

Two windows on one PC (two cables / ports):

```bash
./scripts/launch-booth.sh
# or:
./build/apps/wx/RocketBox --port 0 &
./build/apps/wx/RocketBox --port 1 &
```

Optional identity configs: `demo-config/` with `--config path/to/file.conf`.

There is **no** `--transport sim` on wx today — use the PWA or SDKs for simulation.

---

## 5. C++ SDK / fabric-tunnel against the simulator

Build (sim support is on by default in `sdks/cpp`):

```bash
cmake -S . -B build
cmake --build build -j --target rocketbox-tunnel
```

With `simulated-hardware` already running:

```bash
# Example: two tunnel instances on sim ports 3 and 4
sudo ./build/fabric-tunnel/rocketbox-tunnel --transport sim --port 4 --expose 8081
sudo ./build/fabric-tunnel/rocketbox-tunnel --transport sim --port 3
```

Real hardware: omit `--transport sim` (USB is the default). See [fabric-tunnel/README.md](../fabric-tunnel/README.md).

---

## 6. What to test

### Automated (no cable)

```bash
# Native unit + integration (CMake)
cmake -S . -B build -DBUILD_WX_GUI=OFF
cmake --build build -j
ctest --test-dir build -L unit --output-on-failure
ctest --test-dir build -L integration --output-on-failure

# PWA / TS
cd apps/web && npm ci && npm test
```

Start the sim daemon before SDK integration tests that attach with `TransportType::Sim`.

### Manual checklist — simulator

- [ ] Daemon up; dashboard at `:1773` shows ports
- [ ] Two PWA tabs (`port=1` and `port=2`) connect and see each other
- [ ] File send/receive completes; teal **transfer** activity on the PWA sparkline
- [ ] Dashboard shows session/data activity for those ports

### Manual checklist — real hardware

- [ ] udev (Linux) or WinUSB (Windows) applied; device enumerates
- [ ] wx **and/or** PWA (no `simulate`) connect to `1772:0006`
- [ ] Two peers on the fabric exchange a file
- [ ] Disconnect / replug recovers cleanly

### Hardware CTest (needs cables)

```bash
ctest --test-dir build -L hardware --output-on-failure
```

---

## Quick matrix

| Goal | Commands |
|------|----------|
| Sim only, two PWAs | `simulated-hardware` + `apps/web` `npm run dev` + `?simulate=1&port=1\|2` |
| Sim + tunnel | daemon + `rocketbox-tunnel --transport sim --port N` |
| Real USB desktop | build `RocketBox` + udev/WinUSB + cable |
| Real USB PWA | `npm run dev` **without** `simulate` + WebUSB picker |
| CI-style tests | `ctest -L unit` + `apps/web npm test` |

---

## Related docs

| Doc | Role |
|-----|------|
| [BUILD.md](BUILD.md) | Platform build/package details |
| [INSTALL.md](INSTALL.md) | End-user installers |
| [ARCHITECTURE.md](ARCHITECTURE.md) | Layering and runtime graphs |
| [AGENTS.md](../AGENTS.md) | Contributor conventions |
| [simulated-hardware/README.md](../simulated-hardware/README.md) | Sim ports and Docker |
| [fabric-tunnel/README.md](../fabric-tunnel/README.md) | Tunnel options |
