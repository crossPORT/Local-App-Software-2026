# RocketBox Simulated Hardware

Software stand-in for the RocketBox crossport switching block. Speaks the same
4-endpoint control/data protocol used by the TypeScript and C++ SDKs.

| Port | Role |
|------|------|
| **1772/tcp** | Native / C++ SDK clients |
| **1773/tcp** | WebSocket (PWA) + live web dashboard |

## Docker (recommended)

```bash
cd simulated-hardware
docker build -t rocketbox-sim-hw .
docker run --rm -p 1772:1772 -p 1773:1773 rocketbox-sim-hw
```

Or with Compose (Docker Compose V2 plugin):

```bash
cd simulated-hardware
docker compose up --build
```

Dashboard: http://localhost:1773/

### Environment

| Variable | Default | Meaning |
|----------|---------|---------|
| `ROCKETBOX_TCP_PORT` | `1772` | TCP SDK port |
| `ROCKETBOX_WS_PORT` | `1773` | HTTP/WebSocket port |
| `ROCKETBOX_LISTEN_HOST` | `0.0.0.0` | Bind address |

## Local (without Docker)

```bash
cd simulated-hardware
npm install
npm start
# or: npm run dev   # auto-reload on file changes
```

Requires Node.js 18+.

## Clients

- **PWA / WebUSB sim:** `ws://localhost:1773?port=N` (open the PWA with `?simulate=1&port=N`)
- **C++ / rocketbox-tunnel:** TCP `127.0.0.1:1772`

Full clone → sim → PWA → native walkthrough: **[docs/DEV-DEMO.md](../docs/DEV-DEMO.md)**.
