#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-$ROOT_DIR/build/host_qt}"
WEBCAM_INDEX="${2:-0}"
WIDTH="${3:-1280}"
HEIGHT="${4:-720}"
MODEL_PATH="${SENTRIFACE_HOST_QT_ENROLL_MODEL_PATH:-$ROOT_DIR/third_party/models/buffalo_sc/det_500m.onnx}"

APP_PATH="$BUILD_DIR/host/qt_enroll_app/sentriface_enroll_app"

if [[ ! -x "$APP_PATH" ]]; then
  "$ROOT_DIR/tools/build_host_qt_enroll.sh" "$BUILD_DIR"
fi

args=(
  --input=webcam
  --webcam-index="$WEBCAM_INDEX"
  --width="$WIDTH"
  --height="$HEIGHT"
)

if [[ -f "$MODEL_PATH" ]]; then
  args+=(
    --observation=scrfd
    --detector-model="$MODEL_PATH"
  )
fi

exec "$APP_PATH" \
  "${args[@]}"
