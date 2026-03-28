#!/usr/bin/env bash

set -euo pipefail

usage() {
  echo "Usage: tools/webcam_scrfd_series.sh <output_dir> [captures] [interval_sec] [video_device] [score_threshold] [max_num]"
  echo "Example: tools/webcam_scrfd_series.sh artifacts/webcam_series 5 1 /dev/video0 0.02 5"
}

if [[ $# -lt 1 || $# -gt 6 ]]; then
  usage
  exit 1
fi

if ! command -v python3 >/dev/null 2>&1; then
  echo "python3 not found in PATH" >&2
  exit 2
fi

OUTPUT_DIR="$1"
CAPTURES="${2:-5}"
INTERVAL_SEC="${3:-1}"
VIDEO_DEVICE="${4:-/dev/video0}"
SCORE_THRESHOLD="${5:-0.02}"
MAX_NUM="${6:-5}"

mkdir -p "${OUTPUT_DIR}"

for index in $(seq 1 "${CAPTURES}"); do
  RUN_DIR="${OUTPUT_DIR}/capture_$(printf '%03d' "${index}")"
  echo "[${index}/${CAPTURES}] Capturing ${RUN_DIR}"
  tools/webcam_scrfd_smoke.sh "${RUN_DIR}" "${VIDEO_DEVICE}" "${SCORE_THRESHOLD}" "${MAX_NUM}"
  if [[ "${index}" -lt "${CAPTURES}" ]]; then
    sleep "${INTERVAL_SEC}"
  fi
done

python3 - <<'PY' "${OUTPUT_DIR}" "${CAPTURES}" "${INTERVAL_SEC}" "${VIDEO_DEVICE}" "${SCORE_THRESHOLD}" "${MAX_NUM}"
import csv
import statistics
import sys
from pathlib import Path

output_dir = Path(sys.argv[1])
captures = sys.argv[2]
interval_sec = sys.argv[3]
video_device = sys.argv[4]
score_threshold = sys.argv[5]
max_num = sys.argv[6]

rows = []
for capture_dir in sorted(output_dir.glob("capture_*")):
    summary_path = capture_dir / "scrfd_webcam_summary.txt"
    csv_path = capture_dir / "scrfd_webcam.csv"
    if not summary_path.exists() or not csv_path.exists():
      continue
    detections = list(csv.DictReader(csv_path.open()))
    scores = [float(item["score"]) for item in detections]
    max_score = max(scores) if scores else 0.0
    rows.append((capture_dir.name, len(detections), max_score))

summary_lines = [
    f"video_device={video_device}",
    f"captures={captures}",
    f"interval_sec={interval_sec}",
    f"score_threshold={score_threshold}",
    f"max_num={max_num}",
    f"captures_recorded={len(rows)}",
]

if rows:
    max_scores = [item[2] for item in rows]
    summary_lines.extend([
        f"best_max_score={max(max_scores):.6f}",
        f"worst_max_score={min(max_scores):.6f}",
        f"avg_max_score={statistics.mean(max_scores):.6f}",
    ])
else:
    summary_lines.extend([
        "best_max_score=NA",
        "worst_max_score=NA",
        "avg_max_score=NA",
    ])

summary_lines.append("captures_detail=")
for name, detections, max_score in rows:
    summary_lines.append(f"{name},detections={detections},max_score={max_score:.6f}")

summary_path = output_dir / "series_summary.txt"
summary_path.write_text("\n".join(summary_lines) + "\n", encoding="utf-8")
print(summary_path.read_text(encoding="utf-8"), end="")
PY
