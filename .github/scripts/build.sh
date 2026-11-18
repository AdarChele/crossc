#!/usr/bin/env bash
set -euo pipefail

IFS=',' read -ra SRC_FILES <<< "${DISCOVERED_FILES}"

# NDK LLVM toolchain paths
TOOLCHAIN_BIN="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin"
CLANG="$TOOLCHAIN_BIN/$TARGET-clang"
STRIP="$TOOLCHAIN_BIN/llvm-strip"
READELF="$TOOLCHAIN_BIN/llvm-readelf"

# Verify tools exist
for tool in "$CLANG" "$STRIP" "$READELF"; do
  if [[ ! -x "$tool" ]]; then
    echo "::error::Tool not found: $tool"
    exit 1
  fi
done

for src in "${SRC_FILES[@]}"; do
  base=$(basename "$src" .c)
  echo ">>> Building $base"

  mkdir -p "$OUT_DIR/$base"

  "$CLANG" \
    -target "$TARGET" \
    -Os -flto=auto \
    -ffunction-sections -fdata-sections \
    -fPIE \
    -static-pie \
    -Wl,--gc-sections \
    -o "$OUT_DIR/$base/$base" \
    "$src"

  "$STRIP" --strip-all "$OUT_DIR/$base/$base"

  if "$READELF" -d "$OUT_DIR/$base/$base" | grep -q NEEDED; then
    echo "❌ $base has dynamic deps!"
    exit 1
  else
    echo "✅ $base fully static"
  fi
done