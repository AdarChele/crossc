#!/usr/bin/env bash
set -euo pipefail

mkdir -p "$OUT_DIR"
mapfile -t files < <(find "$SRC_DIR" -type f -name '*.c' | sort)

if [[ ${#files[@]} -eq 0 ]]; then
  echo "::error::No .c files found in $SRC_DIR/"
  exit 1
fi

printf 'files=%s\n' "$(IFS=,; echo "${files[*]}")" >> "$GITHUB_OUTPUT"
echo "Found ${#files[@]} source(s):"
printf '  %s\n' "${files[@]}"