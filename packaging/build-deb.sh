#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

APP_NAME="task-killer"
VERSION="1.0.0"
ARCH="$(dpkg --print-architecture)"
STAGE="build/deb-root"
OUT_DIR="dist"
DEB_FILE="${OUT_DIR}/${APP_NAME}_${VERSION}_${ARCH}.deb"

rm -rf "${STAGE}"
mkdir -p \
  "${STAGE}/DEBIAN" \
  "${STAGE}/usr/bin" \
  "${STAGE}/usr/share/applications" \
  "${STAGE}/usr/share/icons/hicolor/scalable/apps" \
  "${STAGE}/usr/share/task-killer/sounds" \
  "${OUT_DIR}"

install -m 755 build/task-killer "${STAGE}/usr/bin/task-killer"
install -m 644 assets/com.taskkiller.app.svg \
  "${STAGE}/usr/share/icons/hicolor/scalable/apps/com.taskkiller.app.svg"
install -m 644 assets/beeb.wav \
  "${STAGE}/usr/share/task-killer/sounds/beeb.wav"

cat > "${STAGE}/usr/share/applications/com.taskkiller.app.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Task Killer
Comment=Minimalist task manager
Exec=/usr/bin/task-killer
Icon=com.taskkiller.app
Terminal=false
Categories=Utility;Office;
StartupNotify=true
EOF
chmod 644 "${STAGE}/usr/share/applications/com.taskkiller.app.desktop"

INSTALLED_KB="$(du -sk "${STAGE}/usr" | cut -f1)"

cat > "${STAGE}/DEBIAN/control" <<EOF
Package: ${APP_NAME}
Version: ${VERSION}
Section: utils
Priority: optional
Architecture: ${ARCH}
Installed-Size: ${INSTALLED_KB}
Depends: libgtk-4-1, libnotify4
Recommends: alsa-utils | pipewire-bin | libcanberra-gtk3-bin | pulseaudio-utils
Maintainer: Task Killer <noreply@local>
Description: Minimalist Apple-styled task manager for Zorin OS
EOF

dpkg-deb --build --root-owner-group "${STAGE}" "${DEB_FILE}"
chmod 644 "${DEB_FILE}" || true
echo "Built: ${DEB_FILE}"
