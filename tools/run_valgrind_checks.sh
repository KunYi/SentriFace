#!/usr/bin/env bash

set -euo pipefail

usage() {
  echo "Usage: tools/run_valgrind_checks.sh [test_name ...]"
  echo "Example: tools/run_valgrind_checks.sh"
  echo "Example: tools/run_valgrind_checks.sh system_logger_test unlock_controller_test"
}

if ! command -v valgrind >/dev/null 2>&1; then
  echo "valgrind not found in PATH" >&2
  exit 1
fi

if [[ ! -d build ]]; then
  echo "build directory not found; please build the project first" >&2
  exit 2
fi

TESTS=("$@")
if [[ ${#TESTS[@]} -eq 0 ]]; then
  TESTS=(
    system_logger_test
    unlock_controller_test
    offline_sequence_runner_v2_test
  )
fi

for test_name in "${TESTS[@]}"; do
  test_path="build/tests/${test_name}"
  if [[ ! -x "${test_path}" ]]; then
    echo "test executable not found: ${test_path}" >&2
    exit 3
  fi

  echo "[valgrind] ${test_name}"
  valgrind \
    --leak-check=full \
    --show-leak-kinds=all \
    --track-origins=yes \
    --error-exitcode=99 \
    "${test_path}"
done
