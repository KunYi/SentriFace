#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 || $# -gt 3 ]]; then
  echo "Usage: tools/log_archive.sh <input_log> [gzip|zstd|auto] [--keep]" >&2
  exit 1
fi

input_path="$1"
mode="${2:-auto}"
keep_original=0

if [[ "${3:-}" == "--keep" ]] || [[ "${2:-}" == "--keep" ]]; then
  keep_original=1
fi

if [[ ! -f "$input_path" ]]; then
  echo "Input log not found: $input_path" >&2
  exit 2
fi

choose_mode() {
  if [[ "$mode" != "auto" ]]; then
    printf '%s\n' "$mode"
    return
  fi
  if command -v zstd >/dev/null 2>&1; then
    printf '%s\n' "zstd"
  else
    printf '%s\n' "gzip"
  fi
}

actual_mode="$(choose_mode)"

case "$actual_mode" in
  gzip)
    output_path="${input_path}.gz"
    gzip -c "$input_path" > "$output_path"
    ;;
  zstd)
    output_path="${input_path}.zst"
    zstd -q -f -c "$input_path" > "$output_path"
    ;;
  *)
    echo "Unsupported compression mode: $actual_mode" >&2
    exit 3
    ;;
esac

if [[ "$keep_original" -eq 0 ]]; then
  rm -f "$input_path"
fi

echo "$output_path"
