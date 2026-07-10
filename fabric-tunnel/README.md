# RocketBox Fabric–TCP Bridge

Bridges the RocketBox fabric (Session / Connection) to a host IP interface so
ordinary TCP/UDP applications can use the cable as a private LAN.

| Fabric port | Address |
|-------------|---------|
| 1 | `10.64.0.1` |
| 2 | `10.64.0.2` |
| 3 | `10.64.0.3` |
| 4 | `10.64.0.4` |

One live fabric circuit at a time (dial-on-demand). Host services are **not**
published on the fabric IP unless listed with `--expose`.

## Requirements

- Linux
- Root or `CAP_NET_ADMIN` (TUN, netns, routes, optional DNAT)
- USB RocketBox with IntelliConnex 4-endpoint firmware (default), or simulated hardware on `127.0.0.1:1772`
- CMake build of this repo (`rocketbox_sdk`, libusb)

## Build

From the repository root:

```bash
cmake -S . -B build -DBUILD_WX_GUI=OFF
cmake --build build --target rocketbox-tunnel
```

Binary: `build/fabric-tunnel/rocketbox-tunnel`

## Run

**USB (default)** — plug in the RocketBox, then:

```bash
sudo ./build/fabric-tunnel/rocketbox-tunnel --port 4 --expose 8081
sudo ./build/fabric-tunnel/rocketbox-tunnel --port 3
```

**Simulation** — start the daemon first (see [`../simulated-hardware/README.md`](../simulated-hardware/README.md)), then pass `--transport sim`:

```bash
sudo ./build/fabric-tunnel/rocketbox-tunnel --transport sim --port 4 --expose 8081
sudo ./build/fabric-tunnel/rocketbox-tunnel --transport sim --port 3
```

### Options

| Option | Description |
|--------|-------------|
| `--port N` | Fabric port 1–4 → `10.64.0.N` |
| `--transport T` | `usb` (default) or `sim` |
| `--expose P[,P…]` | Publish these host TCP/UDP ports on `10.64.0.N` (default: none) |
| `--iface NAME` | TUN name (default `rbN`) |
| `--no-netns` | Keep TUN in the host netns (same-host demos may short-circuit) |
| `--ping M` | ICMP to peer port M over the fabric, then exit |

## System tray (Linux)

With wxWidgets installed, the build also produces `rocketbox-tunnel-tray`:

```bash
cmake --build build --target rocketbox-tunnel-tray
./build/apps/tunnel-tray/rocketbox-tunnel-tray
```

Tray menu: fabric port, USB/sim transport, Start/Stop. Start uses `pkexec` when not root.
Override the helper path with `ROCKETBOX_TUNNEL_PATH`.

## Notes

- Same-host testing uses network namespaces plus a host gateway so traffic
  traverses the fabric instead of the local kernel.
- Concurrent TCP to two different fabric peers is not supported; switching
  peers drops the previous circuit.
