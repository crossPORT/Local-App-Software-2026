# RocketBox App

**USB file transfer for co-located teams** — plug laptops into **RocketBox** hardware (the USB switched fabric). No IP addresses, no accounts, no IT firewall changes. Connect the cable, open **RocketBox App**, and send files.

[![CI](https://github.com/crossPORT/Local-App-Software-2026/actions/workflows/ci.yml/badge.svg)](https://github.com/crossPORT/Local-App-Software-2026/actions/workflows/ci.yml)

## Names

| Name | What it is |
|------|------------|
| **RocketBox** | The hardware — USB fabric device (vendor/product **1772:0006**) and cables |
| **RocketBox App** | The software — desktop and web apps that talk to RocketBox hardware |

Release installers and the Linux binary are named `RocketBox` / `RocketBox-*` for packaging; this document uses **RocketBox App** for the software.

## What it is

**RocketBox App** is the end-user software for RocketBox hardware. Each participant runs the app on their laptop with a RocketBox USB cable attached. The app discovers peers on the fabric, negotiates a session, and transfers files directly over USB — not over the corporate network.

**RocketBox hardware:** one USB connection per laptop in normal use. With two RocketBox cables on one PC, launch RocketBox App twice — each window binds to one cable (see [Using RocketBox App](#using-rocketbox-app)).

## What you get

| Product | Best for | Download |
|---------|----------|----------|
| **Desktop app** | Windows, macOS, Linux — full native UI | [GitHub Releases](https://github.com/crossPORT/Local-App-Software-2026/releases/latest) — `.exe`, `.dmg`, `.deb`, or `.AppImage` |
| **Web app (PWA)** | Chrome/Edge on any OS; host on your HTTPS site | Same Releases page — `RocketBox-pwa-*.zip` |

Both apps do the same job: connect to RocketBox hardware, show who is online, and send/receive files. Pick desktop for an installed app; pick the PWA when you already have an HTTPS internal host and want zero install for users.

## Supported platforms

| Platform | Desktop app | Web PWA |
|----------|-------------|---------|
| **Windows 10/11 (64-bit)** | Installer (`.exe`) | Chrome or Edge over HTTPS |
| **macOS (Apple Silicon)** | Disk image (`.dmg`) | Chrome or Edge over HTTPS |
| **Linux (Ubuntu/Debian 64-bit)** | `.deb` (recommended) or `.AppImage` | Chrome or Edge over HTTPS |

Release builds are published when the team pushes a version tag (e.g. `v0.0.1`). Pushing the tag starts GitHub Actions — creating a release in the UI alone does not build installers. See [docs/DEPLOYMENT.md](docs/DEPLOYMENT.md).

---

## Linux: USB access rule (required, one-time)

On **Linux only**, the operating system blocks apps and browsers from opening RocketBox hardware until a **udev rule** is installed. This is a one-time setup per machine — not needed on Windows or macOS.

**Why:** RocketBox presents as USB device `1772:0006`. Without the rule, RocketBox App or Chrome will fail to open the device (permission denied).

**What to do:**

1. **If you installed the `.deb` package** — the post-install script copies the rule when `/etc/udev/rules.d/99-sls-fabric-usb.rules` is not already present. **Unplug and replug the USB cable once** after install.
2. **If you use the AppImage, or the rule was not installed** — run once (requires admin password):

   ```bash
   sudo cp /usr/share/rocketbox/99-sls-fabric-usb.rules /etc/udev/rules.d/
   sudo udevadm control --reload-rules && sudo udevadm trigger
   ```

   If you only have a source checkout, use the helper script instead:

   ```bash
   ./scripts/setup-usb-access.sh
   ```

   The rule file is also at the repo root as `99-sls-fabric-usb.rules`.

3. **Unplug and replug the RocketBox USB cable** after installing the rule.

The rule file (`99-sls-fabric-usb.rules`) grants access to RocketBox hardware (`1772:0006`) without running the app as root. The same rule is required for the **web PWA on Linux** — Chrome/Edge use the same USB permissions.

---

## Install and first run

### Windows

1. Download `RocketBox-*-setup.exe` from [Releases](https://github.com/crossPORT/Local-App-Software-2026/releases/latest).
2. Run the installer. Windows SmartScreen may warn on unsigned builds — choose to run anyway if your IT policy allows.
3. Launch **RocketBox App** from the Start Menu (installer lists it as RocketBox).
4. Connect the RocketBox USB cable, then start a transfer.

No USB rules file is required on Windows.

### macOS

1. Download `RocketBox-*-macos-*.dmg` from [Releases](https://github.com/crossPORT/Local-App-Software-2026/releases/latest).
2. Open the DMG and drag **RocketBox App** to Applications (shown as RocketBox in the bundle).
3. First launch: if macOS Gatekeeper blocks the app, right-click the app → **Open**.
4. Connect the RocketBox USB cable.

No udev-style rules are required on macOS.

### Linux (desktop)

**Recommended — Debian/Ubuntu package:**

```bash
sudo apt install ./rocketbox_*_amd64.deb
RocketBox
```

The installed binary is `RocketBox` in `/usr/bin/` (RocketBox App). The `.deb` does not add a desktop menu entry today.

After install, complete the [USB access rule](#linux-usb-access-rule-required-one-time) step if the device is not detected (unplug/replug the cable).

**Portable — AppImage:**

```bash
chmod +x RocketBox-*-linux-x64.AppImage
./RocketBox-*-linux-x64.AppImage
```

You must install the [USB access rule](#linux-usb-access-rule-required-one-time) manually when using AppImage. The bundled rule lives inside the AppImage at `usr/share/rocketbox/99-sls-fabric-usb.rules`, or use the repo file / `scripts/setup-usb-access.sh` from a checkout.

### Web app (PWA)

1. Download `RocketBox-pwa-*.zip` from [Releases](https://github.com/crossPORT/Local-App-Software-2026/releases/latest).
2. Unzip on a web server that serves the files over **HTTPS** (WebUSB does not work on `http://` or `file://`).
3. Open the site in **Chrome** or **Edge**.
4. On **Linux**, install the [USB access rule](#linux-usb-access-rule-required-one-time) first.
5. Connect the RocketBox USB cable. When prompted, allow the browser to access the device.

Configure booth identity (display name, port) in the in-app **Settings** — settings are stored in the browser.

---

## Using RocketBox App

1. **Connect** the RocketBox USB cable to the laptop before or after launching the app.
2. **Wait for discovery** — peers on the same fabric appear in the roster when their session is active.
3. **Send a file** — choose a recipient and file; progress is shown until complete.
4. **Two RocketBox cables on one machine** — launch RocketBox App twice. With two devices connected, the second launch shows a **Connect USB** dialog to pick a cable. To skip the dialog, pass `--port 0` and `--port 1`. Optional `--config` loads booth identity from a file (samples in `demo-config/`).

If RocketBox hardware is not detected: confirm the cable is seated, no other program has exclusive USB access, and on Linux that the udev rule is installed and the cable was replugged.

---

## Trade-show / demo setups

Optional sample identity configs for booth setups live in `demo-config/`. They are **not** loaded automatically.

- **Desktop:** launch with `--config path/to/file.conf` (and `--port N` when using `shared.conf` with `[port0]` / `[port1]` sections). Settings edits apply in memory; they are written back to disk only when the app was started with `--config`.
- **PWA:** set display name, team, and receive folder in **Settings** (stored in the browser).

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
