#!/usr/bin/env bash

set -euo pipefail

usage() {
  echo "Usage: tools/webcam_scrfd_smoke.sh <output_dir> [video_device] [score_threshold] [max_num]"
  echo "Example: tools/webcam_scrfd_smoke.sh artifacts/webcam_check /dev/video0 0.02 5"
}

if [[ $# -lt 1 || $# -gt 4 ]]; then
  usage
  exit 1
fi

if ! command -v ffmpeg >/dev/null 2>&1; then
  echo "ffmpeg not found in PATH" >&2
  exit 2
fi

if [[ ! -x ".venv/bin/python" ]]; then
  echo ".venv/bin/python not found; please prepare the project virtualenv first" >&2
  exit 3
fi

if [[ ! -f "third_party/models/buffalo_sc/det_500m.onnx" ]]; then
  echo "SCRFD model not found at third_party/models/buffalo_sc/det_500m.onnx" >&2
  exit 4
fi

OUTPUT_DIR="$1"
VIDEO_DEVICE="${2:-/dev/video0}"
SCORE_THRESHOLD="${3:-0.02}"
MAX_NUM="${4:-5}"

mkdir -p "${OUTPUT_DIR}"

IMAGE_PATH="${OUTPUT_DIR}/webcam_capture.jpg"
CSV_PATH="${OUTPUT_DIR}/scrfd_webcam.csv"
SUMMARY_PATH="${OUTPUT_DIR}/scrfd_webcam_summary.txt"

echo "[1/3] Capturing webcam frame from ${VIDEO_DEVICE}..."
ffmpeg -hide_banner -loglevel error \
  -f v4l2 \
  -input_format mjpeg \
  -video_size 1280x720 \
  -i "${VIDEO_DEVICE}" \
  -frames:v 1 \
  -y \
  "${IMAGE_PATH}"

echo "[2/3] Running SCRFD ONNX..."
.venv/bin/python tools/run_scrfd_onnx.py \
  third_party/models/buffalo_sc/det_500m.onnx \
  "${IMAGE_PATH}" \
  "${CSV_PATH}" \
  --score-threshold "${SCORE_THRESHOLD}" \
  --max-num "${MAX_NUM}"

echo "[3/3] Writing score summary..."
python3 - <<'PY' "${CSV_PATH}" "${SUMMARY_PATH}" "${VIDEO_DEVICE}" "${SCORE_THRESHOLD}" "${MAX_NUM}"
import csv
import sys
from pathlib import Path

csv_path = Path(sys.argv[1])
summary_path = Path(sys.argv[2])
video_device = sys.argv[3]
score_threshold = sys.argv[4]
max_num = sys.argv[5]

rows = list(csv.DictReader(csv_path.open()))
scores = [float(row["score"]) for row in rows]

lines = [
    f"video_device={video_device}",
    f"score_threshold={score_threshold}",
    f"max_num={max_num}",
    f"detections={len(rows)}",
]
if scores:
    lines.extend([
        f"max_score={max(scores):.6f}",
        f"min_score={min(scores):.6f}",
        "scores=" + ",".join(f"{score:.6f}" for score in scores),
    ])
else:
    lines.extend([
        "max_score=NA",
        "min_score=NA",
        "scores=",
    ])

summary_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
print(summary_path.read_text(encoding="utf-8"), end="")
PY

echo "Done. Artifacts saved to: ${OUTPUT_DIR}"
