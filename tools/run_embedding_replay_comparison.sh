#!/usr/bin/env bash

set -euo pipefail

usage() {
  echo "Usage: tools/run_embedding_replay_comparison.sh <artifact_dir> [track_ids] [embedding_vector]"
  echo "Example: tools/run_embedding_replay_comparison.sh artifacts/webcam_bright"
  echo "Example: tools/run_embedding_replay_comparison.sh artifacts/webcam_bright 0,1,2 1.0,0.0,0.0,0.0"
}

if [[ $# -lt 1 || $# -gt 3 ]]; then
  usage
  exit 1
fi

ARTIFACT_DIR="$1"
TRACK_IDS="${2:-}"
EMBEDDING_VECTOR="${3:-1.0,0.0,0.0,0.0}"

FRAME_MANIFEST="${ARTIFACT_DIR}/offline_manifest.txt"
DETECTION_MANIFEST="${ARTIFACT_DIR}/detection_manifest.txt"
TRACKER_TIMELINE="${ARTIFACT_DIR}/offline_tracker_timeline.csv"
MOCK_EMBEDDING_MANIFEST="${ARTIFACT_DIR}/mock_embedding_manifest.txt"
BASELINE_SUMMARY="${ARTIFACT_DIR}/offline_decision_summary.txt"
BASELINE_EVAL="${ARTIFACT_DIR}/offline_decision_evaluation.txt"
MOCK_SUMMARY="${ARTIFACT_DIR}/offline_decision_summary_mock_embedding.txt"
MOCK_EVAL="${ARTIFACT_DIR}/offline_decision_evaluation_mock_embedding.txt"
COMPARISON_TXT="${ARTIFACT_DIR}/embedding_replay_comparison.txt"

if [[ ! -f "${FRAME_MANIFEST}" ]]; then
  echo "frame manifest not found: ${FRAME_MANIFEST}" >&2
  exit 2
fi

if [[ ! -f "${DETECTION_MANIFEST}" ]]; then
  echo "detection manifest not found: ${DETECTION_MANIFEST}" >&2
  exit 3
fi

if [[ ! -x "build/offline_sequence_runner" ]]; then
  echo "build/offline_sequence_runner not found; please build the project first" >&2
  exit 4
fi

echo "[1/5] Running baseline offline replay..."
./build/offline_sequence_runner "${FRAME_MANIFEST}" "${DETECTION_MANIFEST}" > /dev/null
cp offline_tracker_timeline.csv "${TRACKER_TIMELINE}"
python3 tools/summarize_decision_timeline.py offline_decision_timeline.csv "${BASELINE_SUMMARY}" > /dev/null
python3 tools/evaluate_decision_summary.py "${BASELINE_SUMMARY}" "${BASELINE_EVAL}" > /dev/null

if [[ -z "${TRACK_IDS}" ]]; then
  TRACK_IDS="$(python3 - <<'PY'
import csv
from pathlib import Path
rows = list(csv.DictReader(Path("offline_tracker_timeline.csv").open()))
confirmed = sorted({row["track_id"] for row in rows if row["state"] == "1"})
print(",".join(confirmed))
PY
)"
fi

echo "[2/5] Generating mock embedding manifest..."
python3 tools/generate_mock_embedding_manifest.py \
  "${TRACKER_TIMELINE}" \
  "${MOCK_EMBEDDING_MANIFEST}" \
  --track-ids "${TRACK_IDS}" \
  --vector "${EMBEDDING_VECTOR}" \
  --confirmed-only > /dev/null

echo "[3/5] Running embedding-assisted offline replay..."
SENTRIFACE_SHORT_GAP_MERGE_WINDOW_MS=2500 \
  ./build/offline_sequence_runner "${FRAME_MANIFEST}" "${DETECTION_MANIFEST}" "${MOCK_EMBEDDING_MANIFEST}" > /dev/null

echo "[4/5] Summarizing and evaluating mock embedding replay..."
python3 tools/summarize_decision_timeline.py offline_decision_timeline.csv "${MOCK_SUMMARY}" > /dev/null
python3 tools/evaluate_decision_summary.py \
  "${MOCK_SUMMARY}" \
  "${MOCK_EVAL}" \
  --baseline-summary "${BASELINE_SUMMARY}" > /dev/null

echo "[5/5] Writing comparison summary..."
{
  echo "artifact_dir=${ARTIFACT_DIR}"
  echo "track_ids=${TRACK_IDS}"
  echo "embedding_vector=${EMBEDDING_VECTOR}"
  echo
  echo "[baseline]"
  cat "${BASELINE_EVAL}"
  echo
  echo "[mock_embedding]"
  cat "${MOCK_EVAL}"
} > "${COMPARISON_TXT}"

echo "comparison=${COMPARISON_TXT}"
echo "baseline_eval=${BASELINE_EVAL}"
echo "mock_eval=${MOCK_EVAL}"
