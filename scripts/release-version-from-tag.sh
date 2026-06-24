#!/usr/bin/env bash
# Emit ROCKETBOX_RELEASE_TAG and ROCKETBOX_VERSION for cmake / packaging.
# Usage: eval "$(./scripts/release-version-from-tag.sh v0.0.1)"
set -euo pipefail

TAG="${1:-}"
if [[ -z "$TAG" ]]; then
  TAG="$(git describe --tags --exact-match 2>/dev/null || true)"
fi
if [[ -z "$TAG" ]]; then
  TAG="v0.1.0"
fi

VERSION="${TAG#v}"
if [[ -z "$VERSION" ]]; then
  VERSION="$TAG"
fi

printf 'ROCKETBOX_RELEASE_TAG=%q\n' "$TAG"
printf 'ROCKETBOX_VERSION=%q\n' "$VERSION"
