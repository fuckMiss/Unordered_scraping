#!/usr/bin/env bash
set -euo pipefail

SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INSTALL_DIR="${1:-${TANKEYE_INSTALL_DIR:-${HOME}/TankEye1.1}}"
INSTALL_DIR="$(mkdir -p "${INSTALL_DIR}" && cd "${INSTALL_DIR}" && pwd)"

if [[ "${SRC_DIR}" == "${INSTALL_DIR}" ]]; then
    echo "Runtime already in install directory: ${INSTALL_DIR}"
else
    mkdir -p "${INSTALL_DIR}"
    cp -a "${SRC_DIR}/build" "${INSTALL_DIR}/"
    cp -a "${SRC_DIR}/weights" "${INSTALL_DIR}/"
    cp -a "${SRC_DIR}/qt" "${INSTALL_DIR}/"
    cp -a "${SRC_DIR}/launch_tankeye.sh" "${INSTALL_DIR}/"
    cp -a "${SRC_DIR}/install_desktop_launcher.sh" "${INSTALL_DIR}/"
    cp -a "${SRC_DIR}/install_runtime.sh" "${INSTALL_DIR}/"
    cp -a "${SRC_DIR}/README_RUNTIME.md" "${INSTALL_DIR}/"
fi

chmod +x "${INSTALL_DIR}/launch_tankeye.sh"
chmod +x "${INSTALL_DIR}/install_desktop_launcher.sh"
chmod +x "${INSTALL_DIR}/install_runtime.sh"

"${INSTALL_DIR}/install_desktop_launcher.sh"

echo
echo "TankEye1.1 installed to: ${INSTALL_DIR}"
echo "Run from terminal: ${INSTALL_DIR}/launch_tankeye.sh"
echo "Or search TankEye1.1 from the application menu."
