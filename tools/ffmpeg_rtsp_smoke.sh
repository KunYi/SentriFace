#!/usr/bin/env bash

set -euo pipefail

usage() {
  echo "Usage: tools/ffmpeg_rtsp_smoke.sh <rtsp_url> <output_dir> [duration_sec] [fps] [transport]"
  echo "Example: tools/ffmpeg_rtsp_smoke.sh \"rtsp://192.168.1.10/live\" artifacts/rtsp_check 10 1 tcp"
}

if [[ $# -lt 2 || $# -gt 5 ]]; then
  usage
  exit 1
fi

if ! command -v ffmpeg >/dev/null 2>&1; then
  echo "ffmpeg not found in PATH" >&2
  exit 2
fi

if ! command -v ffprobe >/dev/null 2>&1; then
  echo "ffprobe not found in PATH" >&2
  exit 3
fi

RTSP_URL="$1"
OUTPUT_DIR="$2"
DURATION_SEC="${3:-5}"
FPS="${4:-1}"
TRANSPORT="${5:-tcp}"

mkdir -p "${OUTPUT_DIR}"
mkdir -p "${OUTPUT_DIR}/preview_1fps"

echo "[1/4] Probing RTSP stream..."
ffprobe -v error \
  -show_streams \
  -show_format \
  "${RTSP_URL}" \
  > "${OUTPUT_DIR}/stream_info.txt"

echo "[2/4] Capturing preview frame..."
ffmpeg -y \
  -rtsp_transport "${TRANSPORT}" \
  -i "${RTSP_URL}" \
  -frames:v 1 \
  "${OUTPUT_DIR}/preview.jpg"

echo "[3/4] Extracting ${FPS} FPS preview frames for ${DURATION_SEC}s..."
ffmpeg -y \
  -rtsp_transport "${TRANSPORT}" \
  -i "${RTSP_URL}" \
  -t "${DURATION_SEC}" \
  -vf "fps=${FPS}" \
  "${OUTPUT_DIR}/preview_1fps/frame_%03d.jpg"

echo "[4/4] Recording clip for ${DURATION_SEC}s..."
ffmpeg -y \
  -rtsp_transport "${TRANSPORT}" \
  -i "${RTSP_URL}" \
  -t "${DURATION_SEC}" \
  -an \
  -c:v libx264 \
  -pix_fmt yuv420p \
  "${OUTPUT_DIR}/clip.mp4"

cat > "${OUTPUT_DIR}/artifact_manifest.txt" <<EOF
rtsp_url=${RTSP_URL}
duration_sec=${DURATION_SEC}
fps=${FPS}
transport=${TRANSPORT}
generated_files=stream_info.txt,preview.jpg,preview_1fps/,clip.mp4
EOF

echo "Done. Artifacts saved to: ${OUTPUT_DIR}"
