#!/usr/bin/env bash
# Builds a static PJSIP into .deps/pjsip so kk-sip does not depend on a
# distro/AUR package. Re-run is a no-op once the install prefix exists.
set -euo pipefail

# Environment overrides (used by packaging):
#   KKSIP_DEPS   where to put sources and the install prefix (default: <repo>/.deps)
#   PJSIP_SRC    an already unpacked pjproject tree (makepkg, offline builds)
#   PJSIP_LIBS_ONLY=1  skip pjsua and samples (packages don't need them; tests do)
PJSIP_VERSION="${PJSIP_VERSION:-2.17}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEPS="${KKSIP_DEPS:-$ROOT/.deps}"
SRC="${PJSIP_SRC:-$DEPS/src/pjproject-$PJSIP_VERSION}"
PREFIX="$DEPS/pjsip"

if [[ -f "$PREFIX/lib/pkgconfig/libpjproject.pc" && "${1:-}" != "--force" ]]; then
    echo "PJSIP already installed in $PREFIX (use --force to rebuild)"
    exit 0
fi

mkdir -p "$DEPS/src"
if [[ ! -d "$SRC" ]]; then
    SRC="$DEPS/src/pjproject-$PJSIP_VERSION"
    curl -fL "https://github.com/pjsip/pjproject/archive/refs/tags/$PJSIP_VERSION.tar.gz" \
        | tar -xz -C "$DEPS/src"
fi

# Fixes we carry until they are upstream (patches/pjsip-<version>-*.patch).
for p in "$ROOT"/patches/pjsip-"$PJSIP_VERSION"-*.patch; do
    [[ -e "$p" ]] || continue
    if patch -d "$SRC" -p1 -R --dry-run --silent < "$p" >/dev/null 2>&1; then
        continue # already applied
    fi
    echo "Applying $(basename "$p")"
    patch -d "$SRC" -p1 --silent < "$p"
done

cat > "$SRC/pjlib/include/pj/config_site.h" <<'EOF'
/* kk-sip: audio only, room for many accounts. */
#define PJMEDIA_HAS_VIDEO       0
#define PJSUA_MAX_ACC           64
#define PJSUA_MAX_CALLS         16
#define PJMEDIA_HAS_SRTP        1
EOF

cd "$SRC"
if [[ -n "${MSYSTEM:-}" ]]; then
    # Windows (MSYS2 UCRT64): native CMake reads the .pc files, so they need C:/ paths.
    CONF_PREFIX="$(cygpath -m "$PREFIX")"
    FLAGS="-O2"
else
    CONF_PREFIX="$PREFIX"
    # -fPIC: Arch toolchains build PIE executables, static libs must be PIC.
    FLAGS="-O2 -fPIC"
fi
CFLAGS="$FLAGS" CXXFLAGS="$FLAGS" ./configure \
    --prefix="$CONF_PREFIX" \
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
if [[ "${PJSIP_LIBS_ONLY:-0}" == 1 ]]; then
    make -j"$(nproc)" lib
else
    make -j"$(nproc)"
fi
make install
echo "PJSIP $PJSIP_VERSION installed to $PREFIX"
