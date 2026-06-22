# Deployment and CI

## GitHub Actions workflows

| Workflow | Trigger | Purpose |
|----------|---------|---------|
| [ci.yml](../.github/workflows/ci.yml) | PR + push to `main` | Unit/integration tests, wx compile matrix, PWA build |
| [release.yml](../.github/workflows/release.yml) | tag `v*` | wx installers + `RocketBox-pwa-<tag>.zip` |

## PWA zip (tag releases only)

On `v*` tags, `release.yml` runs `scripts/package-pwa.sh` and attaches
`RocketBox-pwa-<tag>.zip` to the GitHub Release (with wx installers).

Local build:

```bash
./scripts/package-pwa.sh
# → RocketBox-pwa.zip at repo root
```

Serve unzipped contents over HTTPS for WebUSB (Chrome/Edge).

## Releases

Tag a version:

```bash
git tag v0.1.0
git push origin v0.1.0
```

The release workflow produces:

| Artifact | Platform |
|----------|----------|
| `.deb` | Linux (Ubuntu/Debian) |
| `.AppImage` | Linux (portable) |
| `.dmg` | macOS (arm64 from CI runner) |
| `-setup.exe` | Windows (NSIS) |

Installers are unsigned in v1. See [INSTALL.md](INSTALL.md) for end-user steps.

## Local verification

```bash
# Native tests
cmake -S . -B build -DBUILD_WX_GUI=OFF && cmake --build build -j
ctest --test-dir build -L unit -L integration --output-on-failure

# PWA with Pages base path
cd apps/web && npm test && npm run build
```

## Repository

- **Remote:** https://github.com/crossPORT/Local-App-Software-2026
- **License:** proprietary ([LICENSE](../LICENSE))
