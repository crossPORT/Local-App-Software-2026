# RocketBox Tunnel on Windows 11

Requires **Wintun** (`wintun.dll` next to `rocketbox-tunnel.exe`; bundled when the
Tunnel component is installed).

**RocketBox App and Tunnel cannot use the same USB cable at the same time.**

## Install

Run `RocketBox-<tag>-setup.exe`. Enable **Tunnel** (includes **Tray** by default).
Uncheck Tray for server-only.

Before upgrading: Quit tray (right-click → Quit tray) and close RocketBox App.
If Setup says “Error opening file for writing” on a `.dll`, something is still
running — Abort, end those tasks, then run Setup again (do not Ignore).

Installer adds:
- Start Menu → **RocketBox App** and **RocketBox Tunnel Tray** (branded icons)
- Desktop → **RocketBox App**
- Startup folder shortcut for the tray (login) when Tray is installed

Tray restores last Enable state from config. Prefer launching Startup with
`--background` so login only shows the tray icon.

## Port identity

Port 1–4 is **detected** from the cable USB serial. Prefer the tray Cable list;
`(in use)` means another app holds the device. With one cable, Enable is enough.

## Tray

- **Close** hides the panel; the tray icon stays.
- **Quit tray** exits the tray app.
- Right-click: Open panel, Enable/Disable, Open log, Quit tray.
- Second Start Menu launch raises the existing panel (single instance).

1. First Enable prompts **UAC once** to start `rocketbox-tunnel-helper` (elevated).
2. Later Enable/Disable talk to that helper over a named pipe — no UAC each time.
3. Keep the helper running after the UAC prompt.

Status: `Connected · Port N` from the cable. Cable picker only if multiple USBs.

Config: `%USERPROFILE%\.config\rocketbox\tunnel-tray.conf`  
Stats: `%PROGRAMDATA%\RocketBox\tunnel-<N>.stats`  
Lock: `%PROGRAMDATA%\RocketBox\tunnel-<N>.lock`  
Log: `%TEMP%\rocketbox\tunnel.log`

If Enable fails with “UAC cancelled”, approve the helper elevation prompt. If the helper
is missing, reinstall with the **Tunnel** component checked.

## CLI

```text
rocketbox-tunnel
rocketbox-tunnel --port 3
```

USB defaults to auto Port from serial. `--port` only disambiguates multiple cables.
Run elevated (UAC). Only one tunnel per Port (lock file).

`--expose SPEC` publishes host TCP/UDP on the fabric IP (`10.64.0.N`); ICMP echo is
always allowed. Empty expose = no host services. Reload by rewriting
`%PROGRAMDATA%\RocketBox\tunnel-N.expose` (tray Apply does this).

The tray helper runs **one** tunnel child. For a second Port (e.g. peer ping), run
another elevated CLI:

```text
rocketbox-tunnel --port 2 --expose 445
```

## Troubleshooting

- **SmartScreen** — unsigned v1; Run anyway if you trust the build.
- **Wintun missing** — reinstall with the Tunnel component checked.
- **USB busy** — close RocketBox App on that cable.
- **Port N tunnel already running** — stop the other tunnel on that Port.
- **CreateAdapter failed / need Admin** — tray must elevate the helper once; or run
  `rocketbox-tunnel-helper.exe` as Administrator, then Enable in the tray.
- **Helper pipe not available** — start `rocketbox-tunnel-helper` elevated; tray will
  also offer UAC on Enable.
- **Ping / SMB blocked** — check expose list, then Windows Firewall for ICMP and the
  service ports on `10.64.0.0/24`.
