#!/usr/bin/env bash

set -euo pipefail

usage() {
  echo "Usage: tools/generate_offline_manifest.sh <frames_dir> <manifest_path> [start_timestamp_ms] [frame_interval_ms] [pixel_format]"
  echo "Example: tools/generate_offline_manifest.sh artifacts/rtsp_check/preview_1fps artifacts/rtsp_check/offline_manifest.txt 0 1000 rgb888"
}

if [[ $# -lt 2 || $# -gt 5 ]]; then
  usage
  exit 1
fi

FRAMES_DIR="$1"
MANIFEST_PATH="$2"
START_TIMESTAMP_MS="${3:-0}"
FRAME_INTERVAL_MS="${4:-1000}"
PIXEL_FORMAT="${5:-rgb888}"

if [[ ! -d "${FRAMES_DIR}" ]]; then
  echo "frames_dir does not exist: ${FRAMES_DIR}" >&2
  exit 2
fi

case "${PIXEL_FORMAT}" in
  rgb888|bgr888|gray8)
    ;;
  *)
    echo "unsupported pixel_format: ${PIXEL_FORMAT}" >&2
    exit 3
    ;;
esac

shopt -s nullglob
frames=("${FRAMES_DIR}"/*.jpg "${FRAMES_DIR}"/*.jpeg "${FRAMES_DIR}"/*.png)
shopt -u nullglob

if [[ ${#frames[@]} -eq 0 ]]; then
  echo "no image files found in: ${FRAMES_DIR}" >&2
  exit 4
fi

mkdir -p "$(dirname "${MANIFEST_PATH}")"
timestamp_ms="${START_TIMESTAMP_MS}"

: > "${MANIFEST_PATH}"
for frame_path in "${frames[@]}"; do
  frame_name="$(basename "${frame_path}")"
  printf "%s %s %d %d %d %s\n" \
    "${frame_name}" \
    "${timestamp_ms}" \
    640 \
    640 \
    3 \
    "${PIXEL_FORMAT}" \
    >> "${MANIFEST_PATH}"
  timestamp_ms=$((timestamp_ms + FRAME_INTERVAL_MS))
done

echo "Generated manifest: ${MANIFEST_PATH}"
echo "Frames listed: ${#frames[@]}"
echo "Reminder: width/height/channels are placeholder defaults for stage-1 validation and should be corrected when real image probing is added."
