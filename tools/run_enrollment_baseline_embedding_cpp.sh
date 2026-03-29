#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 || $# -gt 4 ]]; then
  echo "usage: tools/run_enrollment_baseline_embedding_cpp.sh <enrollment_summary.txt> [person_id] [output_dir] [model_path]" >&2
  exit 1
fi

SUMMARY_PATH="$1"
PERSON_ID="${2:-1000}"
OUTPUT_DIR="${3:-$(dirname "${SUMMARY_PATH}")}"
MODEL_PATH="${4:-third_party/models/buffalo_sc/w600k_mbf.onnx}"
RUNNER="./build/enrollment_artifact_runner"

if [[ ! -f "${SUMMARY_PATH}" ]]; then
  echo "summary not found: ${SUMMARY_PATH}" >&2
  exit 1
fi

if [[ ! -f "${MODEL_PATH}" ]]; then
  echo "embedding model not found: ${MODEL_PATH}" >&2
  exit 1
fi

if [[ ! -x "${RUNNER}" ]]; then
  echo "runner not found: ${RUNNER}" >&2
  exit 1
fi

mkdir -p "${OUTPUT_DIR}"

ARTIFACT_SUMMARY="${OUTPUT_DIR}/baseline_artifact_summary.txt"

"${RUNNER}" \
  "${SUMMARY_PATH}" \
  "${PERSON_ID}" \
  "${ARTIFACT_SUMMARY}" \
  3 \
  512 \
  onnxruntime \
  "${MODEL_PATH}" > /dev/null

echo "artifact_summary=${ARTIFACT_SUMMARY}"
echo "baseline_binary_package=${OUTPUT_DIR}/baseline_artifact_summary.sfbp"
echo "search_index_package=${OUTPUT_DIR}/baseline_artifact_summary.sfsi"
echo "embedding_input_manifest=${OUTPUT_DIR}/baseline_artifact_summary_embedding_input_manifest.txt"
