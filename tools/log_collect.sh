#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 ]]; then
  echo "Usage: tools/log_collect.sh <output_dir> <path1> [path2 ...]" >&2
  exit 1
fi

output_dir="$1"
shift

mkdir -p "$output_dir"
manifest_path="$output_dir/manifest.txt"
: > "$manifest_path"

timestamp_utc="$(date -u +%Y-%m-%dT%H:%M:%SZ)"

for input_path in "$@"; do
  if [[ ! -e "$input_path" ]]; then
    echo "skip missing: $input_path" >> "$manifest_path"
    continue
  fi

  base_name="$(basename "$input_path")"
  target_path="$output_dir/$base_name"

  if [[ -d "$input_path" ]]; then
    cp -R "$input_path" "$target_path"
    size_bytes="$(du -sb "$target_path" | awk '{print $1}')"
    echo "$timestamp_utc dir $input_path -> $target_path size=$size_bytes" >> "$manifest_path"
  else
    cp "$input_path" "$target_path"
    size_bytes="$(wc -c < "$target_path" | tr -d ' ')"
    echo "$timestamp_utc file $input_path -> $target_path size=$size_bytes" >> "$manifest_path"
  fi
done

echo "$output_dir"
