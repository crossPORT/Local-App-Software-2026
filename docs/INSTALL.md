# Installing RocketBox App

Software for **RocketBox** hardware (USB fabric, `1772:0006`). Installers are packaged as `RocketBox-*`; the running product is **RocketBox App**.
Download the latest installers from
[GitHub Releases](https://github.com/crossPORT/Local-App-Software-2026/releases/latest).

## Web app (zip)

1. Download `RocketBox-pwa-*.zip` from
   [GitHub Releases](https://github.com/crossPORT/Local-App-Software-2026/releases/latest)
   or run `./scripts/package-pwa.sh` from a source checkout.
2. Unzip and serve the contents over **HTTPS** (WebUSB requirement), e.g. with your
   internal static host or `npx serve` behind TLS.
3. Open in **Chrome** or **Edge**, connect the RocketBox USB cable, and grant USB access.

`file://` URLs will not work for WebUSB.

On **Linux**, install the udev rule (see [Linux](#linux)) before the browser can open the device.

## Windows

1. Download `RocketBox-*-setup.exe` (NSIS installer from CPack).
2. Run the installer (SmartScreen may warn — unsigned v1 build).
3. Launch **RocketBox App** from the Start Menu (listed as RocketBox).
4. Connect the RocketBox USB cable before transferring.

If the device is not detected, bind **WinUSB** for `1772:0006` (one-time per PC).
The installer includes `share/rocketbox/demo-config/WINDOWS-USB.txt` with steps
using the bundled `sls_usb_device.inf` / `SLS_USB_DEVICE.cat` or `zadig-2.9.exe`
([Zadig](https://zadig.akeo.ie/)).

No udev rule is required on Windows.

## macOS

1. Download `RocketBox-*.dmg`.
2. Open the DMG and drag **RocketBox App** to Applications (bundle name RocketBox).
3. First launch: right-click → **Open** if Gatekeeper blocks unsigned builds.
4. Connect the RocketBox USB cable.

Release CI builds on `macos-latest` (Apple Silicon). No udev rule is required on macOS.

## Linux

### USB access rule (required, one-time)

The **RocketBox** hardware (`1772:0006`) needs a udev rule on Linux for RocketBox App and for Chrome/Edge WebUSB.

**`.deb` install:** `cmake/debian/postinst` copies
`/usr/share/rocketbox/99-sls-fabric-usb.rules` → `/etc/udev/rules.d/` when that destination file does not exist yet. Unplug and replug the cable once.

**Manual install** (AppImage, or if the rule was not installed):

```bash
sudo cp /usr/share/rocketbox/99-sls-fabric-usb.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules && sudo udevadm trigger
```

From a source checkout: `./scripts/setup-usb-access.sh` (installs the repo-root rule file).

AppImage users without a `.deb` install: use the repo rule file, the helper script, or extract
`usr/share/rocketbox/99-sls-fabric-usb.rules` from the AppImage bundle.

### Debian package (recommended on Ubuntu/Debian)

```bash
sudo apt install ./rocketbox_*_amd64.deb
RocketBox
```

Binary name is **`RocketBox`** in `/usr/bin/` (RocketBox App). The `.deb` package does not install a `.desktop` menu entry.

### AppImage (portable)

```bash
chmod +x RocketBox-*-linux-x64.AppImage
./RocketBox-*-linux-x64.AppImage
```

Install the udev rule manually (see above). The AppImage build includes `rocketbox.desktop` for its own launcher only.

## RocketBox hardware and multiple cables

- **RocketBox** USB device: vendor/product `1772:0006`
- One RocketBox App window per cable
- Two RocketBox units on one PC: launch RocketBox App twice. With two devices connected, each launch prompts to pick a cable unless you pass `--port 0` / `--port 1` on the command line.

Desktop CLI options today: `--config FILE`, `--port N` (see `apps/wx/main.cpp`).

See [protocols/file-transfer.md](../protocols/file-transfer.md) for transfer behavior.
