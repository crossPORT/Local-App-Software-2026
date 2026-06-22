# Installing RocketBox

Download the latest installers from
[GitHub Releases](https://github.com/crossPORT/Local-App-Software-2026/releases/latest).

## Web app (zip)

1. Download `RocketBox-pwa*.zip` from
   [GitHub Releases](https://github.com/crossPORT/Local-App-Software-2026/releases/latest)
   or run `./scripts/package-pwa.sh` from a source checkout.
2. Unzip and serve the contents over **HTTPS** (WebUSB requirement), e.g. with your
   internal static host or `npx serve` behind TLS.
3. Open in **Chrome** or **Edge**, connect the fabric USB cable, and grant USB access.

`file://` URLs will not work for WebUSB.

## Windows

1. Download `RocketBox-*-windows-x64-setup.exe`.
2. Run the installer (SmartScreen may warn — unsigned v1 build).
3. Launch **RocketBox** from the Start Menu.
4. Connect the fabric USB cable before transferring.

If the device is not detected, ensure no other app holds the USB interface and retry.

## macOS

1. Download `RocketBox-*-macos-*.dmg`.
2. Open the DMG and drag **RocketBox** to Applications.
3. First launch: right-click → **Open** if Gatekeeper blocks unsigned builds.
4. Connect the fabric USB cable.

## Linux

### Debian package (recommended on Ubuntu/Debian)

```bash
sudo apt install ./RocketBox-*-linux-x64.deb
rocketbox   # or launch from menu if .desktop added later
```

The package post-install script may install the udev rule for device access. Unplug and replug the cable once.

Manual udev setup:

```bash
sudo cp /usr/share/rocketbox/99-sls-fabric-usb.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules && sudo udevadm trigger
```

### AppImage (portable)

```bash
chmod +x RocketBox-*-linux-x64.AppImage
./RocketBox-*-linux-x64.AppImage
```

Run `scripts/setup-usb-access.sh` from a source checkout if udev rules are not installed.

## USB device

- Vendor/product: `1772:0006`
- One RocketBox app instance per USB cable
- Two cables on one PC: run two app instances with `--port 0` and `--port 1`

See [protocols/file-transfer.md](../protocols/file-transfer.md) for transfer behavior.
