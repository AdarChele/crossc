#!/usr/bin/env bash
set -euo pipefail

echo ">>> Purging $SRC_DIR/ ..."
find "$SRC_DIR" -mindepth 1 -delete
ls -la "$SRC_DIR"/ || true
echo "✅ Source directory emptied"