#!/usr/bin/env bash

set -euo pipefail

usage() {
  echo "Usage: tools/inspect_insightface_package.sh <package.zip> [extract_dir]"
  echo "Example: tools/inspect_insightface_package.sh third_party/models/buffalo_sc.zip third_party/models/buffalo_sc"
}

if [[ $# -lt 1 || $# -gt 2 ]]; then
  usage
  exit 1
fi

PACKAGE_ZIP="$1"
EXTRACT_DIR="${2:-}"

if [[ ! -f "${PACKAGE_ZIP}" ]]; then
  echo "package not found: ${PACKAGE_ZIP}" >&2
  exit 2
fi

if ! command -v unzip >/dev/null 2>&1; then
  echo "unzip not found in PATH" >&2
  exit 3
fi

echo "[1/3] Archive contents"
unzip -l "${PACKAGE_ZIP}"

echo
echo "[2/3] ONNX files"
unzip -Z1 "${PACKAGE_ZIP}" | grep -Ei '\.onnx$' || true

echo
echo "[3/3] Heuristic detector candidates"
unzip -Z1 "${PACKAGE_ZIP}" | grep -Ei '(det|scrfd).*\.(onnx)$' || true

if [[ -n "${EXTRACT_DIR}" ]]; then
  mkdir -p "${EXTRACT_DIR}"
  unzip -o "${PACKAGE_ZIP}" -d "${EXTRACT_DIR}" >/dev/null
  echo
  echo "Extracted to: ${EXTRACT_DIR}"
fi
