# RocketBox Tunnel on Linux

Private LAN over a RocketBox USB cable. Silkscreen **Port** comes from the cable
USB serial → address `10.64.0.N`. Do not invent a Port for USB; it is detected.

**RocketBox App and Tunnel cannot use the same USB cable at the same time.**

## Install

Use the RocketBox Linux installer (`.deb` / AppImage) and enable the **Tunnel**
component. **Tray** is selected by default with Tunnel; uncheck Tray for
server-only (CLI + helper).

Tray autostarts at login with `--background` (icon only). App menu launch opens
the control panel. Restores last Enable state.

Docs install to `/usr/share/doc/rocketbox/tunnel/linux.md` (deb).

## Port identity

Port 1–4 is **detected** from the cable USB serial (same as RocketBox App). The tray
lists plugged cables only; with one cable there is nothing to pick.

## Tray

- **Close** hides the panel; the tray icon stays.
- **Quit tray** (panel or right-click menu) exits the tray app.
- Right-click: Open panel, Enable/Disable, Open log, Quit tray.
- Left-click / second launch: show or raise the panel (single instance).

1. Enable tunnel from the panel or tray menu.
2. USB: Cable from serial (picker only if multiple cables).
3. Optionally check Expose → Apply (SIGHUP reload; no restart).
4. Status shows `Connected · Port N`. Hover for up/down rates.

Config: `~/.config/rocketbox/tunnel-tray.conf`  
Stats: `/run/rocketbox/tunnel-<N>.stats`  
Lock: `/run/rocketbox/tunnel-<N>.lock` (one bridge per Port)  
Log: `/tmp/rocketbox/tunnel.log` (tray **Open log**, or `ROCKETBOX_TUNNEL_LOG`)

## CLI / server

```bash
sudo rocketbox-tunnel                 # USB: Port from cable serial
sudo rocketbox-tunnel --port 3        # USB: disambiguate if multiple cables
sudo rocketbox-tunnel --port 3 --expose tcp:22,445
sudo rocketbox-tunnel --transport sim --port 4
```

| Flag | Meaning |
|------|---------|
| *(none)* | USB auto — one available cable → Port from serial |
| `--port N` | USB: pick cable that is Port N; **required for sim** |
| `--expose SPEC` | Publish host ports on the RocketBox IP |
| `--no-netns` | Keep TUN in the host netns |
| `--transport usb` | USB (default) |

Only one `rocketbox-tunnel` may bridge a given Port (lock file). Privilege: tray
asks for a password **once** to start `rocketbox-tunnel-helper`; Enable / Disable /
Apply then use `/run/rocketbox/helper.sock` with no further prompts while the helper
stays up. Or run the tunnel as root / with `CAP_NET_ADMIN`.

## Troubleshooting

- **already running** — second CLI on same Port; stop the first or use another Port.
- **in use / busy** — close RocketBox App on that cable.
- **auth cancelled** — declined the one-time helper elevation; tunnel stays Off.
- **tray missing** — set `DBUS_SESSION_BUS_ADDRESS` or use `apps/tunnel-tray/run-tray.sh`.
