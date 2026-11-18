#!/usr/bin/env bash
set -euo pipefail

IFS=',' read -ra SRC_FILES <<< "${DISCOVERED_FILES}"

# NDK clang path
CLANG="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin/$TARGET-clang"

for src in "${SRC_FILES[@]}"; do
  base=$(basename "$src" .c)
  echo ">>> Building $base"

  mkdir -p "$OUT_DIR/$base"

  "$CLANG" \
    -target "$TARGET" \
    -Os -flto=auto \
    -ffunction-sections -fdata-sections \
    -fPIE -static \
    -Wl,--gc-sections -pie \
    -o "$OUT_DIR/$base/$base" \
    "$src"

  "$TARGET"-strip --strip-all "$OUT_DIR/$base/$base"

  if "$TARGET"-readelf -d "$OUT_DIR/$base/$base" | grep -q NEEDED; then
    echo "❌ $base has dynamic deps!"
    exit 1
  else
    echo "✅ $base fully static"
  fi
done