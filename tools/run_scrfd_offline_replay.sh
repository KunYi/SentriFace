#!/usr/bin/env bash

set -euo pipefail

usage() {
  echo "Usage: tools/run_scrfd_offline_replay.sh <artifact_dir> [model_path]"
  echo "Example: tools/run_scrfd_offline_replay.sh artifacts/rtsp_check"
  echo "Example: tools/run_scrfd_offline_replay.sh artifacts/rtsp_check third_party/models/buffalo_sc/det_500m.onnx"
}

if [[ $# -lt 1 || $# -gt 2 ]]; then
  usage
  exit 1
fi

ARTIFACT_DIR="$1"
MODEL_PATH="${2:-}"

if [[ ! -d "${ARTIFACT_DIR}" ]]; then
  echo "artifact_dir not found: ${ARTIFACT_DIR}" >&2
  exit 2
fi

FRAME_DIR="${ARTIFACT_DIR}/preview_1fps"
FRAME_MANIFEST="${ARTIFACT_DIR}/offline_manifest.txt"
SCRFD_CSV="${ARTIFACT_DIR}/scrfd_output.csv"
DETECTION_MANIFEST="${ARTIFACT_DIR}/detection_manifest.txt"

if [[ ! -d "${FRAME_DIR}" ]]; then
  echo "preview frame dir not found: ${FRAME_DIR}" >&2
  exit 3
fi

if [[ -z "${MODEL_PATH}" ]]; then
  MODEL_PATH="$(tools/resolve_scrfd_model_path.sh third_party/models/buffalo_sc)"
fi

if [[ ! -f "${MODEL_PATH}" ]]; then
  echo "model not found: ${MODEL_PATH}" >&2
  exit 4
fi

if [[ ! -x ".venv/bin/python" ]]; then
  echo ".venv/bin/python not found; please create the project venv first" >&2
  exit 5
fi

if [[ ! -x "build/offline_sequence_runner" ]]; then
  echo "build/offline_sequence_runner not found; please build the project first" >&2
  exit 6
fi

echo "[1/4] Generating frame manifest..."
tools/generate_offline_manifest.sh "${FRAME_DIR}" "${FRAME_MANIFEST}"

echo "[2/4] Running SCRFD ONNX..."
.venv/bin/python tools/run_scrfd_onnx.py \
  "${MODEL_PATH}" \
  "${FRAME_DIR}" \
  "${SCRFD_CSV}"

echo "[3/4] Converting SCRFD CSV to detection manifest..."
.venv/bin/python tools/convert_scrfd_csv_to_manifest.py \
  "${SCRFD_CSV}" \
  "${DETECTION_MANIFEST}" \
  --frame-manifest "${FRAME_MANIFEST}"

echo "[4/4] Running offline sequence replay..."
(
  cd build
  ./offline_sequence_runner "../${FRAME_MANIFEST}" "../${DETECTION_MANIFEST}"
)

echo
echo "Completed SCRFD offline replay workflow."
echo "frame_manifest=${FRAME_MANIFEST}"
echo "scrfd_csv=${SCRFD_CSV}"
echo "detection_manifest=${DETECTION_MANIFEST}"
echo "report=build/offline_sequence_report.txt"
