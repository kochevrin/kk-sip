#!/usr/bin/env bash
# Builds kk-sip_<ver>_amd64.deb and kk-sip-<ver>-x86_64.AppImage on Ubuntu 24.04
# (used by .github/workflows/release.yml; also runs in `docker run ubuntu:24.04`).
#
#   packaging/linux/build-ubuntu.sh --install-deps              # apt-get the build dependencies first
#   packaging/linux/build-ubuntu.sh --install-deps --deps-only  # only install them (CI test job)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${OUT:-$ROOT/dist}"
BUILD="$ROOT/build-ubuntu"
TOOLS="$BUILD/tools"

if [[ "${1:-}" == "--install-deps" ]]; then
    export DEBIAN_FRONTEND=noninteractive
    SUDO=$([[ $EUID -eq 0 ]] && echo "" || echo sudo)
    $SUDO apt-get update
    $SUDO apt-get install -y --no-install-recommends \
        build-essential cmake pkg-config curl ca-certificates file dpkg-dev \
        qt6-base-dev qt6-base-dev-tools qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools \
        qt6-svg-dev libqt6svg6 qt6-wayland libqt6sql6-sqlite libgl-dev \
        libopus-dev libssl-dev libasound2-dev uuid-dev
    [[ "${2:-}" == "--deps-only" ]] && exit 0
fi

PJSIP_LIBS_ONLY=1 "$ROOT/scripts/build-pjsip.sh"

cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build "$BUILD" -j"$(nproc)"
mkdir -p "$OUT"

# --- .deb (dependencies resolved by dpkg-shlibdeps) ---
(cd "$BUILD" && cpack -G DEB)
mv "$BUILD"/kk-sip_*.deb "$OUT/"

# --- AppImage ---
mkdir -p "$TOOLS"
for tool in linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage \
            linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage; do
    f="$TOOLS/$(basename "$tool")"
    [[ -x "$f" ]] || { curl -fsSL "https://github.com/$tool" -o "$f"; chmod +x "$f"; }
done
APPDIR="$BUILD/AppDir"
rm -rf "$APPDIR"
DESTDIR="$APPDIR" cmake --install "$BUILD"

export APPIMAGE_EXTRACT_AND_RUN=1          # no FUSE in containers/CI
export QMAKE=/usr/lib/qt6/bin/qmake
export EXTRA_PLATFORM_PLUGINS="libqwayland-generic.so;libqoffscreen.so"  # offscreen: headless checks
export EXTRA_QT_PLUGINS="svg;iconengines;imageformats;sqldrivers"
VERSION="$(sed -n 's/^project(kk-sip VERSION \([0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt")"
export LINUXDEPLOY_OUTPUT_VERSION="$VERSION"
(cd "$BUILD" && "$TOOLS/linuxdeploy-x86_64.AppImage" --appdir "$APPDIR" --plugin qt --output appimage \
    --desktop-file "$APPDIR/usr/share/applications/kk-sip.desktop" \
    --icon-file "$APPDIR/usr/share/icons/hicolor/256x256/apps/kk-sip.png")
mv "$BUILD"/kk-sip-*.AppImage "$OUT/"
ls -la "$OUT"
