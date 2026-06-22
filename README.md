# RocketBox

USB file transfer for the SLS fabric FPGA device (`1772:0006`). Co-located teams connect over a USB switched fabric — no IP, no credentials, no IT setup.

[![CI](https://github.com/crossPORT/Local-App-Software-2026/actions/workflows/ci.yml/badge.svg)](https://github.com/crossPORT/Local-App-Software-2026/actions/workflows/ci.yml)

| App | Use |
|-----|-----|
| **Web (PWA)** | [crossport.github.io/Local-App-Software-2026](https://crossport.github.io/Local-App-Software-2026/) — Chrome/Edge + WebUSB |
| **Desktop (wx)** | [GitHub Releases](https://github.com/crossPORT/Local-App-Software-2026/releases/latest) — Windows, macOS, Linux installers |

## Quick start (Linux desktop)

```bash
sudo apt install cmake g++ pkg-config libusb-1.0-0-dev libwxgtk3.2-dev
cmake -S . -B build
cmake --build build -j
./scripts/setup-usb-access.sh   # once, then replug cable
./build/apps/wx/RocketBox --config booth-port0.conf
```

## Quick start (web dev)

```bash
cd apps/web && npm ci && npm run dev
```

Production build (GitHub Pages base path):

```bash
cd apps/web && VITE_BASE_PATH=/Local-App-Software-2026/ npm run build
```

## Tests

```bash
cmake -S . -B build -DBUILD_WX_GUI=OFF && cmake --build build -j
ctest --test-dir build -L unit --output-on-failure
ctest --test-dir build -L integration --output-on-failure
cd apps/web && npm test
```

Hardware tests (`ctest -L hardware`) need two USB cables and auto-skip when absent.

## Repository layout

```
core/           USB engine (fabric_usb_core, libusb, ROCKETBX protocol)
apps/demo/      Session orchestration (TransferOrchestrator, handshake)
apps/wx/        RocketBox desktop UI (primary GUI)
apps/web/       RocketBox PWA (WebUSB)
sim/            In-process fabric simulator for integration tests
tools/          usb-probe, loopback-test, booth-cli
protocols/      Agent guides for session + file transfer
docs/           Architecture, protocol, build, deployment
```

## Documentation

| Document | Contents |
|----------|----------|
| [docs/BUILD.md](docs/BUILD.md) | Build wx on Linux, macOS, Windows |
| [docs/INSTALL.md](docs/INSTALL.md) | End-user install from release installers |
| [docs/DEPLOYMENT.md](docs/DEPLOYMENT.md) | CI, GitHub Pages, releases |
| [protocols/session.md](protocols/session.md) | Session handshake best practices |
| [protocols/file-transfer.md](protocols/file-transfer.md) | Payload transfer best practices |
| [AGENTS.md](AGENTS.md) | Contributor / agent onboarding |

## License

Proprietary — see [LICENSE](LICENSE). `RocketBox-spec.md` is confidential product requirements.
