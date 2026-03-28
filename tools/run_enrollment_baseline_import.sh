#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 || $# -gt 3 ]]; then
  echo "usage: tools/run_enrollment_baseline_import.sh <enrollment_summary.txt> [person_id] [output_dir]" >&2
  exit 1
fi

SUMMARY_PATH="$1"
PERSON_ID="${2:-1000}"
OUTPUT_DIR="${3:-$(dirname "${SUMMARY_PATH}")}"

EMBED_WORKFLOW="tools/run_enrollment_baseline_embedding.sh"
IMPORT_RUNNER="./build/enrollment_baseline_import_runner"

if [[ ! -f "${SUMMARY_PATH}" ]]; then
  echo "summary not found: ${SUMMARY_PATH}" >&2
  exit 1
fi

if [[ ! -x "${EMBED_WORKFLOW}" ]]; then
  echo "workflow not found: ${EMBED_WORKFLOW}" >&2
  exit 1
fi

if [[ ! -x "${IMPORT_RUNNER}" ]]; then
  echo "runner not found: ${IMPORT_RUNNER}" >&2
  exit 1
fi

mkdir -p "${OUTPUT_DIR}"

"${EMBED_WORKFLOW}" "${SUMMARY_PATH}" "${PERSON_ID}" "${OUTPUT_DIR}" > /dev/null

EMBEDDING_CSV="${OUTPUT_DIR}/baseline_embedding_output.csv"
IMPORT_SUMMARY="${OUTPUT_DIR}/baseline_import_summary.txt"

"${IMPORT_RUNNER}" \
  "${EMBEDDING_CSV}" \
  "${PERSON_ID}" \
  "${IMPORT_SUMMARY}" \
  512 > /dev/null

echo "embedding_output_csv=${EMBEDDING_CSV}"
echo "baseline_import_summary=${IMPORT_SUMMARY}"
