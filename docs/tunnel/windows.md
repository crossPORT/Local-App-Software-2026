# RocketBox Tunnel on Windows 11

Requires **Wintun** (`wintun.dll` next to `rocketbox-tunnel.exe`; bundled when the
Tunnel component is installed).

**RocketBox App and Tunnel cannot use the same USB cable at the same time.**

## Install

Run `RocketBox-<tag>-setup.exe`. Enable **Tunnel** (includes **Tray** by default).
Uncheck Tray for server-only.

Tray restores last Enable state from config. Login autostart is via a Startup
shortcut when you enable it (Run-key registration lands in a follow-up).

## Port identity

Port 1–4 is **detected** from the cable USB serial. Prefer the tray Cable list;
`(in use)` means another app holds the device. With one cable, Enable is enough.

## Tray

Notification-area icon → Enable tunnel (UAC once unless helper is running).
Status: `Connected · Port N` from the cable. Cable picker only if multiple USBs.

Config: `%USERPROFILE%\.config\rocketbox\tunnel-tray.conf`  
Stats: `%PROGRAMDATA%\RocketBox\tunnel-<N>.stats`  
Log: `%TEMP%\rocketbox\tunnel.log`

## CLI

```text
rocketbox-tunnel
rocketbox-tunnel --port 3
```

USB defaults to auto Port from serial. `--port` only disambiguates multiple cables.
Run elevated (UAC). `--expose` is not enabled on Windows yet.

## Troubleshooting

- **SmartScreen** — unsigned v1; Run anyway if you trust the build.
- **Wintun missing** — reinstall with the Tunnel component checked.
- **USB busy** — close RocketBox App on that cable.
- **CreateAdapter failed** — run elevated; another Wintun adapter name may conflict.
