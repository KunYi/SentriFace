#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-$ROOT_DIR/build/host_qt_debug}"
ONNXRUNTIME_ROOT="$ROOT_DIR/third_party/onnxruntime/onnxruntime-linux-x64-1.24.4"

cmake_args=(
  -S "$ROOT_DIR"
  -B "$BUILD_DIR"
  -DSENTRIFACE_BUILD_TESTS=OFF
  -DSENTRIFACE_BUILD_HOST_QT_ENROLL=ON
)

if [[ -d "$ONNXRUNTIME_ROOT" ]]; then
  cmake_args+=(
    -DSENTRIFACE_ENABLE_ONNXRUNTIME=ON
    -DSENTRIFACE_ONNXRUNTIME_ROOT="$ONNXRUNTIME_ROOT"
  )
fi

cmake "${cmake_args[@]}"
cmake --build "$BUILD_DIR" --target sentriface_enroll_app

echo "build_dir=$BUILD_DIR"
echo "app_path=$BUILD_DIR/host/qt_enroll_app/sentriface_enroll_app"
