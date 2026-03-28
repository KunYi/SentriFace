#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: tools/log_inspect.sh <log_path> [output_dir]" >&2
  exit 1
fi

log_path="$1"
output_dir="${2:-}"

if [[ ! -f "$log_path" ]]; then
  echo "Log file not found: $log_path" >&2
  exit 2
fi

if [[ -z "$output_dir" ]]; then
  base_name="$(basename "$log_path")"
  output_dir="artifacts/log_inspect/${base_name}.inspect"
fi

mkdir -p "$output_dir"

stats_txt="$output_dir/stats.txt"
stats_json="$output_dir/stats.json"
logcat_txt="$output_dir/logcat.txt"
threadtime_txt="$output_dir/threadtime.txt"

tools/logcat_dump.py "$log_path" --stats --stats-format text > "$stats_txt"
tools/logcat_dump.py "$log_path" --stats --stats-format json > "$stats_json"
tools/logcat_dump.py "$log_path" --output-format logcat --color never > "$logcat_txt"
tools/logcat_dump.py "$log_path" --output-format threadtime --color never > "$threadtime_txt"

tools/log_collect.sh "$output_dir/bundle" "$log_path" "$stats_txt" "$stats_json" "$logcat_txt" "$threadtime_txt" >/dev/null

echo "$output_dir"
