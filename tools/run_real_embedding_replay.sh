#!/usr/bin/env bash

set -euo pipefail

usage() {
  echo "Usage: tools/run_real_embedding_replay.sh <artifact_dir> [recognizer_model] [score_threshold]"
  echo "Example: tools/run_real_embedding_replay.sh artifacts/webcam_bright"
  echo "Example: tools/run_real_embedding_replay.sh artifacts/webcam_bright third_party/models/buffalo_sc/w600k_mbf.onnx 0.60"
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
BASELINE_SUMMARY="${ARTIFACT_DIR}/offline_decision_summary.txt"
BASELINE_EVAL="${ARTIFACT_DIR}/offline_decision_evaluation.txt"
EMBEDDING_CSV="${ARTIFACT_DIR}/embedding_output.csv"
REAL_EMBEDDING_MANIFEST="${ARTIFACT_DIR}/embedding_manifest_real.txt"
REAL_SUMMARY="${ARTIFACT_DIR}/offline_decision_summary_real_embedding.txt"
REAL_EVAL="${ARTIFACT_DIR}/offline_decision_evaluation_real_embedding.txt"
COMPARISON_TXT="${ARTIFACT_DIR}/real_embedding_replay_comparison.txt"

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

echo "[1/6] Running baseline offline replay..."
./build/offline_sequence_runner "${FRAME_MANIFEST}" "${DETECTION_MANIFEST}" > /dev/null
cp offline_tracker_timeline.csv "${TRACKER_TIMELINE}"
python3 tools/summarize_decision_timeline.py offline_decision_timeline.csv "${BASELINE_SUMMARY}" > /dev/null
python3 tools/evaluate_decision_summary.py "${BASELINE_SUMMARY}" "${BASELINE_EVAL}" > /dev/null

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

echo "[4/6] Running real-embedding-assisted offline replay..."
SENTRIFACE_SHORT_GAP_MERGE_WINDOW_MS=2500 \
  ./build/offline_sequence_runner "${FRAME_MANIFEST}" "${DETECTION_MANIFEST}" "${REAL_EMBEDDING_MANIFEST}" > /dev/null

echo "[5/6] Summarizing and evaluating real embedding replay..."
python3 tools/summarize_decision_timeline.py offline_decision_timeline.csv "${REAL_SUMMARY}" > /dev/null
python3 tools/evaluate_decision_summary.py \
  "${REAL_SUMMARY}" \
  "${REAL_EVAL}" \
  --baseline-summary "${BASELINE_SUMMARY}" > /dev/null

echo "[6/6] Writing comparison summary..."
{
  echo "artifact_dir=${ARTIFACT_DIR}"
  echo "recognizer_model=${RECOGNIZER_MODEL}"
  echo "score_threshold=${SCORE_THRESHOLD}"
  echo
  echo "[baseline]"
  cat "${BASELINE_EVAL}"
  echo
  echo "[real_embedding]"
  cat "${REAL_EVAL}"
} > "${COMPARISON_TXT}"

echo "comparison=${COMPARISON_TXT}"
echo "baseline_eval=${BASELINE_EVAL}"
echo "real_eval=${REAL_EVAL}"
