#!/usr/bin/env bash
set -euo pipefail

APP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_BIN="${APP_DIR}/build/yolov11-tensorrt_qt_app"
OBB_ENGINE="${APP_DIR}/weights/best_obb.engine"
SEG_ENGINE="${APP_DIR}/weights/best_seg.engine"
LOG_DIR="${APP_DIR}/logs"

export LD_LIBRARY_PATH="/opt/TensorRT-8.6.1.6/lib:/home/cll/下载/opencv/build/lib:${LD_LIBRARY_PATH:-}"

mkdir -p "${LOG_DIR}"
cd "${APP_DIR}"

if [[ ! -x "${APP_BIN}" ]]; then
    echo "Qt app executable not found: ${APP_BIN}" >> "${LOG_DIR}/tankeye-launch.log"
    exit 1
fi

if [[ ! -f "${OBB_ENGINE}" ]]; then
    echo "OBB engine not found: ${OBB_ENGINE}" >> "${LOG_DIR}/tankeye-launch.log"
    exit 1
fi

if [[ ! -f "${SEG_ENGINE}" ]]; then
    echo "SEG engine not found: ${SEG_ENGINE}" >> "${LOG_DIR}/tankeye-launch.log"
    exit 1
fi

exec "${APP_BIN}" "${OBB_ENGINE}" "${SEG_ENGINE}"
