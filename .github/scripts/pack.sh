#!/usr/bin/env bash
set -euo pipefail

cd "$OUT_DIR"
tar -cf - * | zstd -9 -o "$RELEASE_NAME.tar.zst"
ls -lh "$RELEASE_NAME.tar.zst"
echo "archive=$OUT_DIR/$RELEASE_NAME.tar.zst" >> "$GITHUB_OUTPUT"