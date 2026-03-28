#!/usr/bin/env bash

set -euo pipefail

usage() {
  echo "Usage: tools/resolve_scrfd_model_path.sh <package_dir>"
  echo "Example: tools/resolve_scrfd_model_path.sh third_party/models/buffalo_sc"
}

if [[ $# -ne 1 ]]; then
  usage
  exit 1
fi

PACKAGE_DIR="$1"

if [[ ! -d "${PACKAGE_DIR}" ]]; then
  echo "package_dir not found: ${PACKAGE_DIR}" >&2
  exit 2
fi

MODEL_PATH="$(find "${PACKAGE_DIR}" -maxdepth 2 -type f \( -name 'det_*.onnx' -o -name 'scrfd*.onnx' \) | sort | head -n 1)"

if [[ -z "${MODEL_PATH}" ]]; then
  echo "no SCRFD detector candidate found under: ${PACKAGE_DIR}" >&2
  exit 3
fi

echo "${MODEL_PATH}"
