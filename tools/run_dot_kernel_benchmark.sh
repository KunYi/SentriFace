#!/usr/bin/env bash

set -euo pipefail

usage() {
  echo "Usage: tools/run_dot_kernel_benchmark.sh [prototypes] [iterations] [seed]"
  echo "Example: tools/run_dot_kernel_benchmark.sh"
  echo "Example: tools/run_dot_kernel_benchmark.sh 5000 300 20260328"
}

if [[ $# -gt 3 ]]; then
  usage
  exit 1
fi

if [[ ! -x "build/dot_kernel_benchmark_runner" ]]; then
  echo "build/dot_kernel_benchmark_runner not found; please build the project first" >&2
  exit 2
fi

PROTOTYPES="${1:-1000}"
ITERATIONS="${2:-200}"
SEED="${3:-20260328}"

./build/dot_kernel_benchmark_runner "${PROTOTYPES}" "${ITERATIONS}" "${SEED}"
