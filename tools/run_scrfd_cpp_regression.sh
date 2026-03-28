#!/usr/bin/env bash

set -euo pipefail

usage() {
  echo "Usage: tools/run_scrfd_cpp_regression.sh <input_image> <output_dir> [score_threshold] [max_num]"
  echo "Example: tools/run_scrfd_cpp_regression.sh artifacts/webcam_smoke/webcam_capture.jpg artifacts/scrfd_regression 0.02 5"
}

if [[ $# -lt 2 || $# -gt 4 ]]; then
  usage
  exit 1
fi

if ! command -v ffmpeg >/dev/null 2>&1; then
  echo "ffmpeg not found in PATH" >&2
  exit 2
fi

INPUT_IMAGE="$1"
OUTPUT_DIR="$2"
SCORE_THRESHOLD="${3:-0.02}"
MAX_NUM="${4:-5}"
MODEL_PATH="third_party/models/buffalo_sc/det_500m.onnx"

mkdir -p "${OUTPUT_DIR}"

PPM_PATH="${OUTPUT_DIR}/input.ppm"
PY_CSV="${OUTPUT_DIR}/python_scrfd.csv"
CPP_CSV="${OUTPUT_DIR}/cpp_scrfd.csv"
SUMMARY_PATH="${OUTPUT_DIR}/regression_summary.txt"

if [[ ! -f "${MODEL_PATH}" ]]; then
  echo "Model not found: ${MODEL_PATH}" >&2
  exit 3
fi

if [[ ! -x "build_ort/scrfd_ppm_runner" ]]; then
  echo "build_ort/scrfd_ppm_runner not found; build with SENTRIFACE_ENABLE_ONNXRUNTIME=ON first" >&2
  exit 4
fi

echo "[1/4] Converting input image to PPM..."
ffmpeg -hide_banner -loglevel error -y \
  -i "${INPUT_IMAGE}" \
  -vf "scale=640:640:force_original_aspect_ratio=decrease,pad=640:640:(ow-iw)/2:(oh-ih)/2:black" \
  "${PPM_PATH}"

echo "[2/4] Running Python SCRFD..."
.venv/bin/python tools/run_scrfd_onnx.py \
  "${MODEL_PATH}" \
  "${PPM_PATH}" \
  "${PY_CSV}" \
  --score-threshold "${SCORE_THRESHOLD}" \
  --max-num "${MAX_NUM}"

echo "[3/4] Running C++ SCRFD..."
./build_ort/scrfd_ppm_runner \
  "${MODEL_PATH}" \
  "${PPM_PATH}" \
  "${CPP_CSV}" \
  "${SCORE_THRESHOLD}" \
  0.40 \
  "${MAX_NUM}"

echo "[4/4] Writing regression summary..."
python3 - <<'PY' "${PY_CSV}" "${CPP_CSV}" "${SUMMARY_PATH}"
import csv
import math
import sys
from pathlib import Path

py_csv = Path(sys.argv[1])
cpp_csv = Path(sys.argv[2])
summary_path = Path(sys.argv[3])

def load_rows(path: Path):
    rows = list(csv.DictReader(path.open()))
    for row in rows:
        row["x"] = float(row["x"])
        row["y"] = float(row["y"])
        row["w"] = float(row["w"])
        row["h"] = float(row["h"])
        row["score"] = float(row["score"])
        row["landmarks"] = [
            (float(row[f"l{index}x"]), float(row[f"l{index}y"]))
            for index in range(5)
        ]
    return rows

def rect_iou(a, b):
    ax1, ay1, ax2, ay2 = a["x"], a["y"], a["x"] + a["w"], a["y"] + a["h"]
    bx1, by1, bx2, by2 = b["x"], b["y"], b["x"] + b["w"], b["y"] + b["h"]
    ix1 = max(ax1, bx1)
    iy1 = max(ay1, by1)
    ix2 = min(ax2, bx2)
    iy2 = min(ay2, by2)
    iw = max(0.0, ix2 - ix1)
    ih = max(0.0, iy2 - iy1)
    inter = iw * ih
    union = a["w"] * a["h"] + b["w"] * b["h"] - inter
    return inter / union if union > 0.0 else 0.0

def landmark_mean_distance(a, b):
    distances = []
    for (ax, ay), (bx, by) in zip(a["landmarks"], b["landmarks"]):
        distances.append(math.hypot(ax - bx, ay - by))
    return sum(distances) / len(distances) if distances else 0.0

def greedy_match(py_rows, cpp_rows):
    matches = []
    used_cpp = set()
    for py_index, py_row in enumerate(py_rows):
        best_cpp_index = None
        best_iou = -1.0
        for cpp_index, cpp_row in enumerate(cpp_rows):
            if cpp_index in used_cpp:
                continue
            iou = rect_iou(py_row, cpp_row)
            if iou > best_iou:
                best_iou = iou
                best_cpp_index = cpp_index
        if best_cpp_index is not None:
            used_cpp.add(best_cpp_index)
            cpp_row = cpp_rows[best_cpp_index]
            matches.append({
                "py_index": py_index,
                "cpp_index": best_cpp_index,
                "iou": best_iou,
                "score_delta": abs(py_row["score"] - cpp_row["score"]),
                "landmark_mean_distance": landmark_mean_distance(py_row, cpp_row),
            })
    return matches

py_rows = load_rows(py_csv)
cpp_rows = load_rows(cpp_csv)
py_scores = [row["score"] for row in py_rows]
cpp_scores = [row["score"] for row in cpp_rows]

py_max = max(py_scores) if py_scores else 0.0
cpp_max = max(cpp_scores) if cpp_scores else 0.0
matches = greedy_match(py_rows, cpp_rows)
avg_iou = sum(item["iou"] for item in matches) / len(matches) if matches else 0.0
avg_score_delta = (
    sum(item["score_delta"] for item in matches) / len(matches) if matches else 0.0
)
avg_landmark_distance = (
    sum(item["landmark_mean_distance"] for item in matches) / len(matches)
    if matches else 0.0
)

lines = [
    f"python_detections={len(py_rows)}",
    f"cpp_detections={len(cpp_rows)}",
    f"python_max_score={py_max:.6f}",
    f"cpp_max_score={cpp_max:.6f}",
    f"max_score_delta={abs(py_max - cpp_max):.6f}",
    f"matched_pairs={len(matches)}",
    f"avg_iou={avg_iou:.6f}",
    f"avg_score_delta={avg_score_delta:.6f}",
    f"avg_landmark_distance={avg_landmark_distance:.6f}",
]

summary_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
print(summary_path.read_text(encoding="utf-8"), end="")
PY
