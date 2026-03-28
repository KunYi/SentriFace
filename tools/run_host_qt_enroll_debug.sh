#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-$ROOT_DIR/build/host_qt_debug}"
WEBCAM_INDEX="${2:-0}"
WIDTH="${3:-1280}"
HEIGHT="${4:-720}"
LOG_DIR="${5:-$BUILD_DIR}"

mkdir -p "$LOG_DIR"

STDOUT_LOG="$LOG_DIR/host_qt_enroll_stdout.txt"
STDERR_LOG="$LOG_DIR/host_qt_enroll_stderr.txt"
APP_LOG="$LOG_DIR/host_qt_enroll_app.log"

export SENTRIFACE_LOG_ENABLE=1
export SENTRIFACE_LOG_BACKEND=file
export SENTRIFACE_LOG_FILE="$APP_LOG"
export SENTRIFACE_LOG_LEVEL=debug
export SENTRIFACE_LOG_MODULES=host_preview,host_observation,host_ui

echo "app_log=$APP_LOG"
echo "stdout_log=$STDOUT_LOG"
echo "stderr_log=$STDERR_LOG"

"$ROOT_DIR/tools/build_host_qt_enroll.sh" "$BUILD_DIR" >/dev/null

"$ROOT_DIR/tools/run_host_qt_enroll_webcam.sh" "$BUILD_DIR" "$WEBCAM_INDEX" "$WIDTH" "$HEIGHT" \
  > >(tee "$STDOUT_LOG") \
  2> >(tee "$STDERR_LOG" >&2)
