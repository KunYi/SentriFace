#!/usr/bin/env bash

set -euo pipefail

usage() {
  echo "Usage: tools/download_insightface_asset.sh <asset_name> [output_dir]"
  echo "Example: tools/download_insightface_asset.sh buffalo_sc.zip"
  echo "Supported assets:"
  echo "  antelopev2.zip"
  echo "  buffalo_l.zip"
  echo "  buffalo_m.zip"
  echo "  buffalo_s.zip"
  echo "  buffalo_sc.zip"
  echo "  inswapper_128.onnx"
  echo "  scrfd_person_2.5g.onnx"
}

if [[ $# -lt 1 || $# -gt 2 ]]; then
  usage
  exit 1
fi

ASSET_NAME="$1"
OUTPUT_DIR="${2:-third_party/models}"
BASE_URL="https://github.com/deepinsight/insightface/releases/download/v0.7"

case "${ASSET_NAME}" in
  antelopev2.zip|buffalo_l.zip|buffalo_m.zip|buffalo_s.zip|buffalo_sc.zip|inswapper_128.onnx|scrfd_person_2.5g.onnx)
    ;;
  *)
    echo "unsupported asset: ${ASSET_NAME}" >&2
    usage
    exit 2
    ;;
esac

mkdir -p "${OUTPUT_DIR}"
TARGET_PATH="${OUTPUT_DIR}/${ASSET_NAME}"
TMP_PATH="${TARGET_PATH}.part"
URL="${BASE_URL}/${ASSET_NAME}"

echo "Downloading: ${URL}"
curl -L --fail --progress-bar -o "${TMP_PATH}" "${URL}"
mv "${TMP_PATH}" "${TARGET_PATH}"
echo "Saved to: ${TARGET_PATH}"
