# RocketBox Tunnel on macOS

Private LAN over a RocketBox USB cable (`10.64.0.N` for silkscreen Port N).
Uses **utun** (no Linux netns). No session announce.

**RocketBox App and Tunnel cannot use the same USB cable at the same time.**

## Install

Install from the RocketBox **DMG** (same release as the App). Enable Tunnel
(+ Tray by default). Uncheck Tray for server-only CLI.

Tray `.app` uses ad-hoc codesign (same as App). First launch may need
right-click → Open. Tray autostarts at login and restores last Enable state.

## Port identity

Port 1–4 is derived from the USB serial. The tray lists present cables;
`(in use)` means the App (or another process) holds the device.

## Tray

Enable tunnel → pick Port → optional expose presets when available.
Status: `Connected · Port N`.

Config: `~/.config/rocketbox/tunnel-tray.conf`  
Stats: `$TMPDIR/rocketbox/tunnel-<N>.stats`

## CLI

```bash
sudo rocketbox-tunnel --port 3 --no-netns
```

`--expose` / pf NAT is not enabled in the macOS MVP — omit expose or use Linux.
Default is `--no-netns` (required).

## Troubleshooting

- **Gatekeeper** — right-click → Open on first run.
- **USB busy** — quit RocketBox App on that cable.
- **utun / route errors** — run with administrator privileges.
