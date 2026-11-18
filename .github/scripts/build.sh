#!/usr/bin/env bash
set -euo pipefail

IFS=',' read -ra SRC_FILES <<< "${DISCOVERED_FILES}"

for src in "${SRC_FILES[@]}"; do
  base=$(basename "$src" .c)
  echo ">>> Building $base"

  cmake -B "$OUT_DIR/$base" -S . \
    -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-30 \
    -DCMAKE_BUILD_TYPE=MinSizeRel \
    -DCMAKE_C_FLAGS="-Os -flto=auto -ffunction-sections -fdata-sections -fPIE -static" \
    -DCMAKE_EXE_LINKER_FLAGS="-Wl,--gc-sections -static -pie -flto=auto" \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_C_SOURCE_FILE="$src" \
    -DCMAKE_PROJECT_NAME="$base"

  cmake --build "$OUT_DIR/$base" -j$(nproc)

  "$TARGET"-strip --strip-all "$OUT_DIR/$base/$base"

  if "$TARGET"-readelf -d "$OUT_DIR/$base/$base" | grep -q NEEDED; then
    echo "❌ $base has dynamic deps!"
    exit 1
  else
    echo "✅ $base fully static"
  fi
done