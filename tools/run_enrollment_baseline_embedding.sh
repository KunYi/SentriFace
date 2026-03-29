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
IMPORT_RUNNER="./build/enrollment_baseline_import_runner"
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

if [[ ! -x "${IMPORT_RUNNER}" ]]; then
  echo "runner not found: ${IMPORT_RUNNER}" >&2
  exit 1
fi

if [[ ! -x "${PYTHON_BIN}" ]]; then
  echo "python not found: ${PYTHON_BIN}" >&2
  exit 1
fi

mkdir -p "${OUTPUT_DIR}"

ARTIFACT_SUMMARY="${OUTPUT_DIR}/baseline_artifact_summary.txt"
"${RUNNER}" "${SUMMARY_PATH}" "${PERSON_ID}" "${ARTIFACT_SUMMARY}" 3 512 mock > /dev/null

INPUT_MANIFEST="${OUTPUT_DIR}/baseline_artifact_summary_embedding_input_manifest.txt"
OUTPUT_CSV="${OUTPUT_DIR}/baseline_embedding_output.csv"
IMPORT_SUMMARY="${OUTPUT_DIR}/baseline_import_summary.txt"
BASELINE_PACKAGE="${OUTPUT_DIR}/baseline_embedding_output.sfbp"
SEARCH_INDEX_PACKAGE="${OUTPUT_DIR}/baseline_embedding_output.sfsi"

"${PYTHON_BIN}" tools/run_baseline_embedding_onnx.py \
  "${MODEL_PATH}" \
  "${INPUT_MANIFEST}" \
  "${OUTPUT_CSV}"

"${IMPORT_RUNNER}" \
  "${OUTPUT_CSV}" \
  "${PERSON_ID}" \
  "${IMPORT_SUMMARY}" \
  512 > /dev/null

echo "artifact_summary=${ARTIFACT_SUMMARY}"
echo "baseline_import_summary=${IMPORT_SUMMARY}"
echo "baseline_binary_package=${BASELINE_PACKAGE}"
echo "search_index_package=${SEARCH_INDEX_PACKAGE}"
echo "embedding_input_manifest=${INPUT_MANIFEST}"
echo "embedding_output_csv=${OUTPUT_CSV}"
