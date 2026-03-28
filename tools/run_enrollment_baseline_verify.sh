#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 || $# -gt 3 ]]; then
  echo "usage: tools/run_enrollment_baseline_verify.sh <enrollment_summary.txt> [person_id] [output_dir]" >&2
  exit 1
fi

SUMMARY_PATH="$1"
PERSON_ID="${2:-1000}"
OUTPUT_DIR="${3:-$(dirname "${SUMMARY_PATH}")}"

IMPORT_WORKFLOW="tools/run_enrollment_baseline_import.sh"
VERIFY_RUNNER="./build/enrollment_baseline_verify_runner"

if [[ ! -f "${SUMMARY_PATH}" ]]; then
  echo "summary not found: ${SUMMARY_PATH}" >&2
  exit 1
fi

if [[ ! -x "${IMPORT_WORKFLOW}" ]]; then
  echo "workflow not found: ${IMPORT_WORKFLOW}" >&2
  exit 1
fi

if [[ ! -x "${VERIFY_RUNNER}" ]]; then
  echo "runner not found: ${VERIFY_RUNNER}" >&2
  exit 1
fi

mkdir -p "${OUTPUT_DIR}"

"${IMPORT_WORKFLOW}" "${SUMMARY_PATH}" "${PERSON_ID}" "${OUTPUT_DIR}" > /dev/null

EMBEDDING_CSV="${OUTPUT_DIR}/baseline_embedding_output.csv"
VERIFY_SUMMARY="${OUTPUT_DIR}/baseline_verify_summary.txt"

"${VERIFY_RUNNER}" \
  "${EMBEDDING_CSV}" \
  "${PERSON_ID}" \
  "${VERIFY_SUMMARY}" \
  512 > /dev/null

echo "embedding_output_csv=${EMBEDDING_CSV}"
echo "baseline_verify_summary=${VERIFY_SUMMARY}"
