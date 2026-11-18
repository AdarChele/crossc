#!/usr/bin/env bash
set -euo pipefail

if [[ -z "$RELEASE_ARCHIVE" || ! -f "$RELEASE_ARCHIVE" ]]; then
  echo "::error::RELEASE_ARCHIVE not found: $RELEASE_ARCHIVE"
  exit 1
fi

TAG="${GITHUB_REF_NAME}"
REPO="${GITHUB_REPOSITORY}"

# Check if release exists
if gh release view "$TAG" --repo "$REPO" >/dev/null 2>&1; then
  echo "Release exists — uploading/overwriting asset..."
  gh release upload "$TAG" "$RELEASE_ARCHIVE" --clobber --repo "$REPO"
else
  echo "Release not found — creating new..."
  gh release create "$TAG" \
    --title "$RELEASE_NAME @ $TAG" \
    --notes "**Android arm64 API 30 static binaries**
- Compiled with NDK $NDK_VERSION
- \`-Os -flto=auto -static-pie -fno-stack-protector\` + \`--strip-all\` + \`--gc-sections\`
- Compressed: \`tar --zstd -9\`
- Sources purged post-build" \
    "$RELEASE_ARCHIVE" \
    --latest \
    --repo "$REPO"
fi