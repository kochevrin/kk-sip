#!/usr/bin/env bash
# Builds a static PJSIP into .deps/pjsip so kk-sip does not depend on a
# distro/AUR package. Re-run is a no-op once the install prefix exists.
set -euo pipefail

PJSIP_VERSION="${PJSIP_VERSION:-2.17}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEPS="$ROOT/.deps"
SRC="$DEPS/src/pjproject-$PJSIP_VERSION"
PREFIX="$DEPS/pjsip"

if [[ -f "$PREFIX/lib/pkgconfig/libpjproject.pc" && "${1:-}" != "--force" ]]; then
    echo "PJSIP already installed in $PREFIX (use --force to rebuild)"
    exit 0
fi

mkdir -p "$DEPS/src"
if [[ ! -d "$SRC" ]]; then
    curl -fL "https://github.com/pjsip/pjproject/archive/refs/tags/$PJSIP_VERSION.tar.gz" \
        | tar -xz -C "$DEPS/src"
fi

cat > "$SRC/pjlib/include/pj/config_site.h" <<'EOF'
/* kk-sip: audio only, room for many accounts. */
#define PJMEDIA_HAS_VIDEO       0
#define PJSUA_MAX_ACC           64
#define PJSUA_MAX_CALLS         16
#define PJMEDIA_HAS_SRTP        1
EOF

cd "$SRC"
# -fPIC: Arch toolchains build PIE executables, static libs must be PIC.
CFLAGS="-O2 -fPIC" CXXFLAGS="-O2 -fPIC" ./configure \
    --prefix="$PREFIX" \
    --disable-video \
    --disable-v4l2 \
    --disable-sdl \
    --disable-ffmpeg \
    --disable-openh264 \
    --disable-libyuv \
    --disable-libwebrtc \
    --disable-android-mediacodec \
    --disable-darwin-ssl \
    --disable-opencore-amr \
    --disable-upnp

make dep
make -j"$(nproc)"
make install
echo "PJSIP $PJSIP_VERSION installed to $PREFIX"
