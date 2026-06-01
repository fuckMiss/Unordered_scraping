#!/usr/bin/env bash
set -euo pipefail

APP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LAUNCH_SCRIPT="${APP_DIR}/launch_tankeye.sh"
ICON_FILE="${APP_DIR}/qt/assets/app_icon.png"

DESKTOP_DIR="${XDG_DESKTOP_DIR:-}"
if [[ -z "${DESKTOP_DIR}" || ! -d "${DESKTOP_DIR}" ]]; then
    if [[ -d "${HOME}/桌面" ]]; then
        DESKTOP_DIR="${HOME}/桌面"
    else
        DESKTOP_DIR="${HOME}/Desktop"
    fi
fi

mkdir -p "${DESKTOP_DIR}"
mkdir -p "${HOME}/.local/share/applications"
chmod +x "${LAUNCH_SCRIPT}"

write_launcher() {
    local launcher_path="$1"
    cat > "${launcher_path}" <<EOF
[Desktop Entry]
Type=Application
Name=TankEye1.1
Comment=截止阀无序抓取上料视觉检测系统
Exec=${LAUNCH_SCRIPT}
Icon=${ICON_FILE}
Path=${APP_DIR}
Terminal=false
Categories=Science;
StartupNotify=true
EOF
    chmod +x "${launcher_path}"
}

write_launcher "${DESKTOP_DIR}/TankEye.desktop"
write_launcher "${HOME}/.local/share/applications/tankeye.desktop"

if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "${HOME}/.local/share/applications" >/dev/null 2>&1 || true
fi

echo "Desktop launcher installed: ${DESKTOP_DIR}/TankEye.desktop"
echo "Application menu launcher installed: ${HOME}/.local/share/applications/tankeye.desktop"
