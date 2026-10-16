#!/usr/bin/env bash
# Builds a static PJSIP for Android into .deps/android/pjsip/<abi>, one tree per ABI.
# The app's CMake links these libraries into libkksip.so.
#
#   android/scripts/build-pjsip.sh [abi...]      default: arm64-v8a x86_64
#
# Needs ANDROID_NDK_ROOT (or ANDROID_NDK_HOME). Re-run is a no-op for an ABI that is
# already installed; --force rebuilds.
set -euo pipefail

PJSIP_VERSION="${PJSIP_VERSION:-2.17}"
API_LEVEL="${API_LEVEL:-26}" # keep in sync with minSdk in app/build.gradle.kts
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DEPS="${KKSIP_DEPS:-$ROOT/.deps}/android"
TARBALL="$DEPS/src/pjproject-$PJSIP_VERSION.tar.gz"

FORCE=0
ABIS=()
for arg in "$@"; do
    case "$arg" in
        --force) FORCE=1 ;;
        *) ABIS+=("$arg") ;;
    esac
done
[[ ${#ABIS[@]} -gt 0 ]] || ABIS=(arm64-v8a x86_64)

export ANDROID_NDK_ROOT="${ANDROID_NDK_ROOT:-${ANDROID_NDK_HOME:-}}"
if [[ -z "$ANDROID_NDK_ROOT" || ! -d "$ANDROID_NDK_ROOT" ]]; then
    echo "Set ANDROID_NDK_ROOT to the Android NDK" >&2
    exit 1
fi

mkdir -p "$DEPS/src"
if [[ ! -f "$TARBALL" ]]; then
    curl -fL -o "$TARBALL.part" \
        "https://github.com/pjsip/pjproject/archive/refs/tags/$PJSIP_VERSION.tar.gz"
    mv "$TARBALL.part" "$TARBALL"
fi

JOBS="$(getconf _NPROCESSORS_ONLN)"

for ABI in "${ABIS[@]}"; do
    PREFIX="$DEPS/pjsip/$ABI"
    if [[ -f "$PREFIX/lib/pkgconfig/libpjproject.pc" && $FORCE == 0 ]]; then
        echo "PJSIP for $ABI already installed in $PREFIX (use --force to rebuild)"
        continue
    fi

    # configure-android builds in the source tree, so every ABI gets a fresh copy.
    SRC="$DEPS/build/$ABI/pjproject-$PJSIP_VERSION"
    rm -rf "$DEPS/build/$ABI" "$PREFIX"
    mkdir -p "$DEPS/build/$ABI"
    tar -xzf "$TARBALL" -C "$DEPS/build/$ABI"

    # Same fixes as the desktop build (scripts/build-pjsip.sh).
    for p in "$ROOT"/patches/pjsip-"$PJSIP_VERSION"-*.patch; do
        [[ -e "$p" ]] || continue
        echo "Applying $(basename "$p")"
        patch -d "$SRC" -p1 --silent < "$p"
    done

    cat > "$SRC/pjlib/include/pj/config_site.h" <<'EOF'
/* kk-sip for Android: PJSIP's Android defaults, audio only. */
#define PJ_CONFIG_ANDROID 1
#include <pj/config_site_sample.h>

#undef  PJMEDIA_HAS_VIDEO
#define PJMEDIA_HAS_VIDEO       0
#undef  PJSUA_MAX_ACC
#define PJSUA_MAX_ACC           32
#define PJMEDIA_HAS_SRTP        1
/* libkksip.so has its own JNI_OnLoad and hands the JVM over with pj_jni_set_jvm(). */
#define PJ_JNI_HAS_JNI_ONLOAD   0
EOF

    (
        cd "$SRC"
        TARGET_ABI="$ABI" APP_PLATFORM="android-$API_LEVEL" \
        CFLAGS="-O2 -fPIC" CXXFLAGS="-O2 -fPIC" \
        ./configure-android --use-ndk-cflags \
            --prefix="$PREFIX" \
            --disable-video \
            --disable-ssl \
            --disable-sdl \
            --disable-ffmpeg \
            --disable-openh264 \
            --disable-libyuv \
            --disable-libwebrtc \
            --disable-android-mediacodec \
            --disable-opencore-amr \
            --disable-silk \
            --disable-bcg729 \
            --disable-upnp
        make dep
        make -j"$JOBS" lib
        make install
    )
    echo "PJSIP $PJSIP_VERSION for $ABI installed to $PREFIX"
done
