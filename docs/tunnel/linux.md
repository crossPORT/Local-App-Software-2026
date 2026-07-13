# RocketBox Tunnel on Linux

Private LAN over a RocketBox USB cable. Silkscreen **Port** comes from the cable
USB serial → address `10.64.0.N`. Do not invent a Port for USB; it is detected.

**RocketBox App and Tunnel cannot use the same USB cable at the same time.**

## Install

Use the RocketBox Linux installer (`.deb` / AppImage) and enable the **Tunnel**
component. **Tray** is selected by default with Tunnel; uncheck Tray for
server-only (CLI + helper).

Tray registers for session autostart and restores the last Enable state.

Docs install to `/usr/share/doc/rocketbox/tunnel/linux.md` (deb).

## Port identity

Port 1–4 is **detected** from the cable USB serial (same as RocketBox App). The tray
lists plugged cables only; with one cable there is nothing to pick.

## Tray

1. Open the tray icon → Enable tunnel.
2. USB: Cable is detected from serial (picker only if multiple cables).
3. Optionally check services under Expose → Apply (restarts if running).
4. Status shows `Connected · Port N`. Hover the icon for up/down rates.

Config: `~/.config/rocketbox/tunnel-tray.conf`  
Stats: `/run/rocketbox/tunnel-<N>.stats`  
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

Privilege: root/`CAP_NET_ADMIN`, `pkexec` from the tray, or `rocketbox-tunnel-helper`
listening on `/run/rocketbox/helper.sock`.

## Troubleshooting

- **in use / busy** — close RocketBox App on that cable.
- **auth cancelled** — declined pkexec; tunnel stays Off.
- **tray missing** — set `DBUS_SESSION_BUS_ADDRESS` or use `apps/tunnel-tray/run-tray.sh`.
