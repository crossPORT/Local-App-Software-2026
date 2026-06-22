# RocketBox

**USB file transfer for co-located teams** — connect laptops over the SLS fabric FPGA USB device. No IP addresses, no accounts, no IT firewall changes. Plug in the cable, open RocketBox, and send files.

[![CI](https://github.com/crossPORT/Local-App-Software-2026/actions/workflows/ci.yml/badge.svg)](https://github.com/crossPORT/Local-App-Software-2026/actions/workflows/ci.yml)

## What it is

RocketBox is the end-user software for the SLS switched USB fabric. Each participant runs RocketBox on their laptop with a fabric USB cable attached. The app discovers peers on the fabric, negotiates a session, and transfers files directly over USB — not over the corporate network.

**Hardware:** SLS fabric USB device (vendor/product **1772:0006**). One app instance per cable. Two cables on one PC (e.g. local testing): run two instances with `--port 0` and `--port 1`.

## What you get

| Product | Best for | Download |
|---------|----------|----------|
| **Desktop app** | Windows, macOS, Linux — full native UI | [GitHub Releases](https://github.com/crossPORT/Local-App-Software-2026/releases/latest) — `.exe`, `.dmg`, `.deb`, or `.AppImage` |
| **Web app (PWA)** | Chrome/Edge on any OS; host on your HTTPS site | Same Releases page — `RocketBox-pwa-*.zip` |

Both apps do the same job: connect to the fabric, show who is online, and send/receive files. Pick desktop for a installed app; pick the PWA when you already have an HTTPS internal host and want zero install for users.

## Supported platforms

| Platform | Desktop app | Web PWA |
|----------|-------------|---------|
| **Windows 10/11 (64-bit)** | Installer (`.exe`) | Chrome or Edge over HTTPS |
| **macOS (Apple Silicon)** | Disk image (`.dmg`) | Chrome or Edge over HTTPS |
| **Linux (Ubuntu/Debian 64-bit)** | `.deb` (recommended) or `.AppImage` | Chrome or Edge over HTTPS |

Release builds are published when the team tags a version (e.g. `v0.1.0`). Always download from **[GitHub Releases](https://github.com/crossPORT/Local-App-Software-2026/releases/latest)** — pick the latest tag, then download the asset for your platform.

---

## Linux: USB access rule (required, one-time)

On **Linux only**, the operating system blocks normal apps (and browsers) from talking to the fabric USB device until a **udev rule** is installed. This is a one-time setup per machine — not needed on Windows or macOS.

**Why:** The fabric device (`1772:0006`) is a custom USB peripheral. Without the rule, RocketBox or Chrome will fail to open the device (permission denied).

**What to do:**

1. **If you installed the `.deb` package** — the installer usually copies the rule automatically. **Unplug and replug the USB cable once** after install.
2. **If you use the AppImage, or the rule was not installed** — run once (requires admin password):

   ```bash
   sudo cp /usr/share/rocketbox/99-sls-fabric-usb.rules /etc/udev/rules.d/
   sudo udevadm control --reload-rules && sudo udevadm trigger
   ```

   If you only have the zip/source tree, use the helper script instead:

   ```bash
   ./scripts/setup-usb-access.sh
   ```

3. **Unplug and replug the fabric USB cable** after installing the rule.

The rule file (`99-sls-fabric-usb.rules`) grants read/write access to device `1772:0006` without running RocketBox as root. The same rule is required for the **web PWA on Linux** — Chrome/Edge use the same USB permissions.

---

## Install and first run

### Windows

1. Download `RocketBox-*-windows-x64-setup.exe` from [Releases](https://github.com/crossPORT/Local-App-Software-2026/releases/latest).
2. Run the installer. Windows SmartScreen may warn on unsigned builds — choose to run anyway if your IT policy allows.
3. Launch **RocketBox** from the Start Menu.
4. Connect the fabric USB cable, then start a transfer.

No USB rules file is required on Windows.

### macOS

1. Download `RocketBox-*-macos-*.dmg` from [Releases](https://github.com/crossPORT/Local-App-Software-2026/releases/latest).
2. Open the DMG and drag **RocketBox** to Applications.
3. First launch: if macOS Gatekeeper blocks the app, right-click the app → **Open**.
4. Connect the fabric USB cable.

No udev-style rules are required on macOS.

### Linux (desktop)

**Recommended — Debian/Ubuntu package:**

```bash
sudo apt install ./rocketbox_*_amd64.deb
rocketbox
```

After install, complete the [USB access rule](#linux-usb-access-rule-required-one-time) step if the device is not detected (unplug/replug the cable).

**Portable — AppImage:**

```bash
chmod +x RocketBox-*-linux-x64.AppImage
./RocketBox-*-linux-x64.AppImage
```

You must install the [USB access rule](#linux-usb-access-rule-required-one-time) manually when using AppImage.

### Web app (PWA)

1. Download `RocketBox-pwa-*.zip` from [Releases](https://github.com/crossPORT/Local-App-Software-2026/releases/latest).
2. Unzip on a web server that serves the files over **HTTPS** (WebUSB does not work on `http://` or `file://`).
3. Open the site in **Chrome** or **Edge**.
4. On **Linux**, install the [USB access rule](#linux-usb-access-rule-required-one-time) first.
5. Connect the fabric USB cable. When prompted, allow the browser to access the USB device.

Configure booth identity (display name, port) in the in-app **Settings** — settings are stored in the browser.

---

## Using RocketBox

1. **Connect** the fabric USB cable to the laptop before or after launching the app.
2. **Wait for discovery** — peers on the same fabric appear in the roster when their session is active.
3. **Send a file** — choose a recipient and file; progress is shown until complete.
4. **Two cables on one machine** — launch two desktop instances: `RocketBox --port 0` and `RocketBox --port 1`.

If the device is not listed: confirm the cable is seated, no other program has exclusive USB access, and on Linux that the udev rule is installed and the cable was replugged.

---

## Trade-show / demo setups

Optional sample identity configs (display names, port indices) live in `demo-config/` for booth scenarios. They are **not** loaded automatically — copy or import values through app Settings when running a demo.

---

## For developers and contributors

This repository is a monorepo: shared USB engine (`core/`), session logic (`lib/session/`), desktop app (`apps/wx/`), web PWA (`apps/web/`), simulators, and CLI tools.

| Document | Contents |
|----------|----------|
| [docs/INSTALL.md](docs/INSTALL.md) | Install details (mirrors end-user steps above) |
| [docs/BUILD.md](docs/BUILD.md) | Build desktop app from source (Linux, macOS, Windows) |
| [docs/DEPLOYMENT.md](docs/DEPLOYMENT.md) | CI vs release; how installers are published |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Code layout and dependency graph |
| [AGENTS.md](AGENTS.md) | Contributor / agent onboarding |

**Build desktop from source (Linux example):**

```bash
sudo apt install cmake g++ pkg-config libusb-1.0-0-dev libwxgtk3.2-dev
cmake -S . -B build && cmake --build build -j
./scripts/setup-usb-access.sh   # once — installs udev rule; replug cable
./build/apps/wx/RocketBox
```

**Web dev:**

```bash
cd apps/web && npm ci && npm run dev
```

**Tests:**

```bash
cmake -S . -B build -DBUILD_WX_GUI=OFF && cmake --build build -j
ctest --test-dir build -L unit --output-on-failure
cd apps/web && npm test
```

## License

Proprietary — see [LICENSE](LICENSE).
