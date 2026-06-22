# Deployment and CI

## GitHub Actions workflows

| Workflow | Trigger | Purpose |
|----------|---------|---------|
| [ci.yml](../.github/workflows/ci.yml) | PR + push to `main` | Unit/integration tests + PWA build/test (no installers) |
| [release.yml](../.github/workflows/release.yml) | **tag push** `v*` or `*.*.*`, or **manual** workflow dispatch | Linux/macOS/Windows installers + PWA zip → GitHub Release |

## Where builds are kept

**Canonical storage:** [GitHub Releases](https://github.com/crossPORT/Local-App-Software-2026/releases) for each tag.

When you push a version tag (e.g. `v0.0.1` or `0.0.1`), `release.yml`:

1. Builds wx RocketBox on **ubuntu**, **macos**, and **windows** runners (CPack + AppImage on Linux).
2. Builds the PWA zip on ubuntu.
3. Uploads job artifacts (400-day retention backup).
4. Creates/updates the **GitHub Release** for that tag and attaches all installer files.

| File (typical) | Platform | Kept on |
|----------------|----------|---------|
| `rocketbox_*_amd64.deb` | Linux (Debian/Ubuntu) | GitHub Release (permanent) |
| `RocketBox-v*-linux-x64.AppImage` | Linux (portable) | GitHub Release |
| `RocketBox-*.dmg` | macOS (arm64 CI runner) | GitHub Release |
| `RocketBox-*-setup.exe` | Windows (NSIS) | GitHub Release |
| `RocketBox-pwa-v*.zip` | Web PWA | GitHub Release |

Download from: **Releases →** pick tag → **Assets**.

CI on `main` does **not** produce or store desktop installers — only tests and a PWA compile check.

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

Tag a version and push (use a **`v` prefix** or plain semver — both work):

```bash
git tag v0.0.1
git push origin v0.0.1
```

Or without the `v` prefix:

```bash
git tag 0.0.1
git push origin 0.0.1
```

**Creating a release only in the GitHub UI does not build installers.** The workflow runs when the **tag is pushed** to the remote (or when you use **Actions → Release → Run workflow** and enter the tag name).

If you already published an empty release, open **Actions → Release → Run workflow**, enter the tag (e.g. `0.0.1`), and run — it will attach built assets to that release.

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
