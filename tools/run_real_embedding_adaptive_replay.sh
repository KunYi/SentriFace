#!/usr/bin/env bash

set -euo pipefail

usage() {
  echo "Usage: tools/run_real_embedding_adaptive_replay.sh <artifact_dir> [recognizer_model] [score_threshold]"
  echo "Example: tools/run_real_embedding_adaptive_replay.sh artifacts/webcam_bright"
  echo "Example: tools/run_real_embedding_adaptive_replay.sh artifacts/webcam_bright third_party/models/buffalo_sc/w600k_mbf.onnx 0.60"
}

if [[ $# -lt 1 || $# -gt 3 ]]; then
  usage
  exit 1
fi

ARTIFACT_DIR="$1"
RECOGNIZER_MODEL="${2:-third_party/models/buffalo_sc/w600k_mbf.onnx}"
SCORE_THRESHOLD="${3:-0.60}"

FRAME_MANIFEST="${ARTIFACT_DIR}/offline_manifest.txt"
DETECTION_MANIFEST="${ARTIFACT_DIR}/detection_manifest.txt"
DETECTION_CSV="${ARTIFACT_DIR}/scrfd_frames.csv"
TRACKER_TIMELINE="${ARTIFACT_DIR}/offline_tracker_timeline.csv"
EMBEDDING_CSV="${ARTIFACT_DIR}/embedding_output.csv"
REAL_EMBEDDING_MANIFEST="${ARTIFACT_DIR}/embedding_manifest_real.txt"

BASELINE_REPORT="${ARTIFACT_DIR}/offline_sequence_report_v2_baseline.txt"
BASELINE_UPDATES="${ARTIFACT_DIR}/offline_prototype_updates_v2_baseline.csv"
ADAPTIVE_REPORT="${ARTIFACT_DIR}/offline_sequence_report_v2_adaptive.txt"
ADAPTIVE_UPDATES="${ARTIFACT_DIR}/offline_prototype_updates_v2_adaptive.csv"
ADAPTIVE_COMPARE="${ARTIFACT_DIR}/real_embedding_adaptive_replay_comparison.txt"

if [[ ! -f "${FRAME_MANIFEST}" ]]; then
  echo "frame manifest not found: ${FRAME_MANIFEST}" >&2
  exit 2
fi

if [[ ! -f "${DETECTION_MANIFEST}" ]]; then
  echo "detection manifest not found: ${DETECTION_MANIFEST}" >&2
  exit 3
fi

if [[ ! -f "${DETECTION_CSV}" ]]; then
  echo "detection csv not found: ${DETECTION_CSV}" >&2
  exit 4
fi

if [[ ! -f "${RECOGNIZER_MODEL}" ]]; then
  echo "recognizer model not found: ${RECOGNIZER_MODEL}" >&2
  exit 5
fi

if [[ ! -x "build/offline_sequence_runner" ]]; then
  echo "build/offline_sequence_runner not found; please build the project first" >&2
  exit 6
fi

if [[ ! -x ".venv/bin/python" ]]; then
  echo ".venv/bin/python not found; please prepare the Python environment first" >&2
  exit 7
fi

echo "[1/6] Running V2 baseline replay without adaptive updates..."
SENTRIFACE_PIPELINE_USE_V2=1 \
  SENTRIFACE_SHORT_GAP_MERGE_WINDOW_MS=2500 \
  ./build/offline_sequence_runner \
  "${FRAME_MANIFEST}" "${DETECTION_MANIFEST}" "${REAL_EMBEDDING_MANIFEST}" > /dev/null
cp offline_tracker_timeline.csv "${TRACKER_TIMELINE}"
cp offline_sequence_report.txt "${BASELINE_REPORT}"
rm -f "${BASELINE_UPDATES}"

echo "[2/6] Running real face embedding ONNX..."
.venv/bin/python tools/run_face_embedding_onnx.py \
  "${RECOGNIZER_MODEL}" \
  "${DETECTION_CSV}" \
  "${EMBEDDING_CSV}" \
  --score-threshold "${SCORE_THRESHOLD}" > /dev/null

echo "[3/6] Converting embedding CSV into replay manifest..."
python3 tools/convert_embedding_csv_to_manifest.py \
  "${EMBEDDING_CSV}" \
  "${TRACKER_TIMELINE}" \
  "${REAL_EMBEDDING_MANIFEST}" \
  --frame-manifest "${FRAME_MANIFEST}" > /dev/null

echo "[4/6] Running V2 replay with adaptive updates enabled..."
SENTRIFACE_PIPELINE_USE_V2=1 \
  SENTRIFACE_PIPELINE_APPLY_ADAPTIVE_UPDATES=1 \
  SENTRIFACE_SHORT_GAP_MERGE_WINDOW_MS=2500 \
  ./build/offline_sequence_runner \
  "${FRAME_MANIFEST}" "${DETECTION_MANIFEST}" "${REAL_EMBEDDING_MANIFEST}" > /dev/null
cp offline_sequence_report.txt "${ADAPTIVE_REPORT}"
cp offline_prototype_updates.csv "${ADAPTIVE_UPDATES}"

echo "[5/6] Writing adaptive replay comparison..."
BASELINE_ADAPTIVE_UPDATES=$(grep -E '^adaptive_updates_applied=' "${BASELINE_REPORT}" | cut -d'=' -f2)
ADAPTIVE_ADAPTIVE_UPDATES=$(grep -E '^adaptive_updates_applied=' "${ADAPTIVE_REPORT}" | cut -d'=' -f2)
BASELINE_UNLOCKS=$(grep -E '^unlock_actions=' "${BASELINE_REPORT}" | cut -d'=' -f2)
ADAPTIVE_UNLOCKS=$(grep -E '^unlock_actions=' "${ADAPTIVE_REPORT}" | cut -d'=' -f2)

{
  echo "artifact_dir=${ARTIFACT_DIR}"
  echo "recognizer_model=${RECOGNIZER_MODEL}"
  echo "score_threshold=${SCORE_THRESHOLD}"
  echo
  echo "[v2_baseline]"
  cat "${BASELINE_REPORT}"
  echo
  echo "[v2_adaptive]"
  cat "${ADAPTIVE_REPORT}"
  echo
  echo "[delta]"
  echo "adaptive_updates_applied_delta=$((ADAPTIVE_ADAPTIVE_UPDATES - BASELINE_ADAPTIVE_UPDATES))"
  echo "unlock_actions_delta=$((ADAPTIVE_UNLOCKS - BASELINE_UNLOCKS))"
  echo "baseline_update_artifact=${BASELINE_UPDATES}"
  echo "adaptive_update_artifact=${ADAPTIVE_UPDATES}"
} > "${ADAPTIVE_COMPARE}"

echo "[6/6] Done"
echo "comparison=${ADAPTIVE_COMPARE}"
echo "baseline_report=${BASELINE_REPORT}"
echo "adaptive_report=${ADAPTIVE_REPORT}"
echo "adaptive_updates=${ADAPTIVE_UPDATES}"
