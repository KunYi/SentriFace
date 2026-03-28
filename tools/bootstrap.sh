#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PYTHON_BIN="${PYTHON_BIN:-python3}"
VENV_DIR="${VENV_DIR:-$ROOT_DIR/.venv}"
DOWNLOAD_ONNXRUNTIME=0
DOWNLOAD_BUFFALO_SC=0
ONNXRUNTIME_VERSION="${ONNXRUNTIME_VERSION:-1.24.4}"

usage() {
  cat <<EOF
Usage: tools/bootstrap.sh [options]

Options:
  --download-onnxruntime   Download ONNX Runtime C/C++ package under third_party/onnxruntime
  --download-buffalo-sc    Download buffalo_sc.zip under third_party/models
  --python <path>          Python executable to use for venv creation
  -h, --help               Show this help

Environment:
  PYTHON_BIN               Override python executable (default: python3)
  VENV_DIR                 Override venv path (default: .venv)
  ONNXRUNTIME_VERSION      Override ONNX Runtime version (default: 1.24.4)
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --download-onnxruntime)
      DOWNLOAD_ONNXRUNTIME=1
      shift
      ;;
    --download-buffalo-sc)
      DOWNLOAD_BUFFALO_SC=1
      shift
      ;;
    --python)
      PYTHON_BIN="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

cd "$ROOT_DIR"

mkdir -p third_party/models third_party/onnxruntime artifacts

if [[ ! -d "$VENV_DIR" ]]; then
  "$PYTHON_BIN" -m venv "$VENV_DIR"
fi

"$VENV_DIR/bin/python" -m pip install --upgrade pip setuptools wheel
"$VENV_DIR/bin/python" -m pip install -r requirements.txt

if [[ $DOWNLOAD_ONNXRUNTIME -eq 1 ]]; then
  ORT_DIR="third_party/onnxruntime/onnxruntime-linux-x64-${ONNXRUNTIME_VERSION}"
  if [[ ! -d "$ORT_DIR" ]]; then
    TMP_TGZ="third_party/onnxruntime/onnxruntime-linux-x64-${ONNXRUNTIME_VERSION}.tgz"
    URL="https://github.com/microsoft/onnxruntime/releases/download/v${ONNXRUNTIME_VERSION}/onnxruntime-linux-x64-${ONNXRUNTIME_VERSION}.tgz"
    echo "Downloading ONNX Runtime from $URL"
    curl -L --fail --progress-bar -o "$TMP_TGZ" "$URL"
    tar -xzf "$TMP_TGZ" -C third_party/onnxruntime
    rm -f "$TMP_TGZ"
  fi
fi

if [[ $DOWNLOAD_BUFFALO_SC -eq 1 ]]; then
  tools/download_insightface_asset.sh buffalo_sc.zip third_party/models
fi

cat <<EOF
Bootstrap complete.

Venv:
  $VENV_DIR

Key directories:
  third_party/models
  third_party/onnxruntime

Notes:
  - .venv and third_party model/runtime assets are intentionally gitignored.
  - If you downloaded buffalo_sc.zip, extract it under third_party/models before use.

Suggested next steps:
  tools/build_host_qt_enroll.sh
  tools/run_host_qt_enroll_webcam.sh
EOF
