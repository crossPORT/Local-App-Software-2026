#!/usr/bin/env bash
# Emit ROCKETBOX_RELEASE_TAG and ROCKETBOX_VERSION for cmake / packaging.
# Usage: eval "$(./scripts/release-version-from-tag.sh v0.1.46-tunnel)"
# CMake project(VERSION) needs numeric X.Y.Z only; tags may be v0.2.00-waterloo.
set -euo pipefail

TAG="${1:-}"
if [[ -z "$TAG" ]]; then
  TAG="$(git describe --tags --exact-match 2>/dev/null || true)"
fi
if [[ -z "$TAG" ]]; then
  TAG="v0.1.0"
fi

RAW="${TAG#v}"
if [[ -z "$RAW" ]]; then
  RAW="$TAG"
fi

# 0.1.46-tunnel / 1.0.0.waterloo → 0.1.46 / 1.0.0
if [[ "$RAW" =~ ^([0-9]+(\.[0-9]+){0,2}) ]]; then
  VERSION="${BASH_REMATCH[1]}"
else
  VERSION="0.1.0"
fi

printf 'ROCKETBOX_RELEASE_TAG=%q\n' "$TAG"
printf 'ROCKETBOX_VERSION=%q\n' "$VERSION"
