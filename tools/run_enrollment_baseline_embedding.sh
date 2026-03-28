#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 || $# -gt 3 ]]; then
  echo "usage: tools/run_enrollment_baseline_embedding.sh <enrollment_summary.txt> [person_id] [output_dir]" >&2
  exit 1
fi

SUMMARY_PATH="$1"
PERSON_ID="${2:-1000}"
OUTPUT_DIR="${3:-$(dirname "${SUMMARY_PATH}")}"

MODEL_PATH="third_party/models/buffalo_sc/w600k_mbf.onnx"
RUNNER="./build/enrollment_artifact_runner"
PYTHON_BIN=".venv/bin/python"

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

if [[ ! -x "${PYTHON_BIN}" ]]; then
  echo "python not found: ${PYTHON_BIN}" >&2
  exit 1
fi

mkdir -p "${OUTPUT_DIR}"

BASE_SUMMARY="${OUTPUT_DIR}/baseline_package_summary_real.txt"
"${RUNNER}" "${SUMMARY_PATH}" "${PERSON_ID}" "${BASE_SUMMARY}" 3 512 mock > /dev/null

INPUT_MANIFEST="${OUTPUT_DIR}/baseline_package_summary_real_embedding_input_manifest.txt"
OUTPUT_CSV="${OUTPUT_DIR}/baseline_embedding_output.csv"

"${PYTHON_BIN}" tools/run_baseline_embedding_onnx.py \
  "${MODEL_PATH}" \
  "${INPUT_MANIFEST}" \
  "${OUTPUT_CSV}"

echo "baseline_summary=${BASE_SUMMARY}"
echo "embedding_input_manifest=${INPUT_MANIFEST}"
echo "embedding_output_csv=${OUTPUT_CSV}"
