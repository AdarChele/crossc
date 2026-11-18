#!/usr/bin/env bash
set -euo pipefail

if [[ -z "$RELEASE_ARCHIVE" || ! -f "$RELEASE_ARCHIVE" ]]; then
  echo "::error::RELEASE_ARCHIVE not found: $RELEASE_ARCHIVE"
  exit 1
fi

gh release create "${GITHUB_REF_NAME}" \
  --title "$RELEASE_NAME @ ${GITHUB_REF_NAME}" \
  --notes "**Android arm64 API 30 static binaries**
- Compiled with NDK $NDK_VERSION
- \`-Os -flto=auto -static -pie\` + \`--strip-all\` + \`--gc-sections\`
- Compressed: \`tar --zstd -9\`
- Sources purged post-build" \
  "$RELEASE_ARCHIVE" \
  --latest \
  --repo "$GITHUB_REPOSITORY"