# RocketBox

USB file transfer for the SLS fabric FPGA device (`1772:0006`). Co-located teams connect over a USB switched fabric — no IP, no credentials, no IT setup.

[![CI](https://github.com/crossPORT/Local-App-Software-2026/actions/workflows/ci.yml/badge.svg)](https://github.com/crossPORT/Local-App-Software-2026/actions/workflows/ci.yml)

| App | Use |
|-----|-----|
| **Web (PWA)** | `RocketBox-pwa.zip` from [Releases](https://github.com/crossPORT/Local-App-Software-2026/releases/latest) or `./scripts/package-pwa.sh` |
| **Desktop (wx)** | [GitHub Releases](https://github.com/crossPORT/Local-App-Software-2026/releases/latest) — Windows, macOS, Linux installers |

## Quick start (Linux desktop)

```bash
sudo apt install cmake g++ pkg-config libusb-1.0-0-dev libwxgtk3.2-dev
cmake -S . -B build
cmake --build build -j
./scripts/setup-usb-access.sh   # once, then replug cable
./build/apps/wx/RocketBox
```

## Quick start (web dev)

```bash
cd apps/web && npm ci && npm run dev
```

Production zip (serve over HTTPS for WebUSB):

```bash
./scripts/package-pwa.sh    # creates RocketBox-pwa.zip at repo root
cd apps/web/dist && python3 -m http.server 8080   # dev serve; use HTTPS in production
```

## Tests

```bash
cmake -S . -B build -DBUILD_WX_GUI=OFF && cmake --build build -j
ctest --test-dir build -L unit --output-on-failure
ctest --test-dir build -L integration --output-on-failure
cd apps/web && npm test    # vitest: session, config, roster, format, …
```

Hardware tests (`ctest -L hardware`) need two USB cables and auto-skip when absent.

## Repository layout

Monorepo convention: **`lib/`** = shared libraries, **`apps/`** = runnable products, **`tools/`** = CLIs, **`tests/`** = C++ test binaries.

```
core/           USB engine (fabric_usb_core, libusb, ROCKETBX protocol)
lib/session/    Shared native session logic (rocketbox_session — handshake, orchestration)
apps/wx/        RocketBox desktop app (wxWidgets UI)
apps/web/       RocketBox PWA (WebUSB — TypeScript, parallel to lib/session)
sim/            In-process fabric simulator for integration tests
tools/          usb-probe, loopback-test, booth-cli, …
demo-config/    Optional sample identity configs (not loaded by default)
protocols/      Agent guides for session + file transfer
docs/           Architecture, protocol, build, deployment
```

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the full dependency graph.

## Documentation

| Document | Contents |
|----------|----------|
| [docs/BUILD.md](docs/BUILD.md) | Build wx on Linux, macOS, Windows |
| [docs/INSTALL.md](docs/INSTALL.md) | End-user install from release installers |
| [docs/DEPLOYMENT.md](docs/DEPLOYMENT.md) | CI vs release; installers live on GitHub Releases (tag `v*` only) |
| [protocols/session.md](protocols/session.md) | Session handshake best practices |
| [protocols/file-transfer.md](protocols/file-transfer.md) | Payload transfer best practices |
| [AGENTS.md](AGENTS.md) | Contributor / agent onboarding |

## License

Proprietary — see [LICENSE](LICENSE). `RocketBox-spec.md` is confidential product requirements.
