# Deployment and CI

## GitHub Actions workflows

| Workflow | Trigger | Purpose |
|----------|---------|---------|
| [ci.yml](../.github/workflows/ci.yml) | PR + push to `main` | Unit/integration tests, wx compile matrix, PWA build |
| [pages.yml](../.github/workflows/pages.yml) | push to `main` | Deploy PWA to GitHub Pages |
| [release.yml](../.github/workflows/release.yml) | tag `v*` | Build wx installers for Linux, macOS, Windows |

## GitHub Pages (PWA only)

- **URL:** https://crossport.github.io/Local-App-Software-2026/
- **Source:** `apps/web/dist` from `pages.yml`
- **Base path:** `VITE_BASE_PATH=/Local-App-Software-2026/` at build time

One-time repo setup: **Settings → Pages → Build and deployment → GitHub Actions**.

Desktop installers are **not** deployed to Pages. They are attached to GitHub Releases only.

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
cd apps/web && npm test && VITE_BASE_PATH=/Local-App-Software-2026/ npm run build
grep -o '/Local-App-Software-2026/[^"]*' dist/index.html | head
```

## Repository

- **Remote:** https://github.com/crossPORT/Local-App-Software-2026
- **License:** proprietary ([LICENSE](../LICENSE))
- **Public repo** recommended for free GitHub Pages with proprietary license
