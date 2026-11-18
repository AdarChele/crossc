#!/usr/bin/env bash
set -euo pipefail

cd "$OUT_DIR"
tar --zstd -cf "$RELEASE_NAME.tar.zst" -9 *
ls -lh "$RELEASE_NAME.tar.zst"
echo "archive=$OUT_DIR/$RELEASE_NAME.tar.zst" >> "$GITHUB_OUTPUT"