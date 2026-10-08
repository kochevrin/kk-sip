#!/usr/bin/env bash
# Builds kk-sip for Windows in an MSYS2 UCRT64 shell and lays out a self-contained
# folder (exe + Qt + MinGW DLLs) in dist/stage, plus a portable ZIP next to it.
# The MSI is made from dist/stage by packaging/windows/build-msi.ps1.
#
#   packaging/windows/build-windows.sh [--install-deps] [--tests]
#
#   --install-deps  pacman -S the compilers and libraries first
#   --tests         also build kk-sip-smoke (offline part runs without a PBX)
set -euo pipefail

if [[ "${MSYSTEM:-}" != UCRT64 ]]; then
    echo "Run this from an MSYS2 UCRT64 shell." >&2
    exit 1
fi

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TESTS=OFF
for arg in "$@"; do
    case "$arg" in
    --install-deps)
        pacman -S --needed --noconfirm make patch curl zip \
            mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,pkgconf,qt6-base,qt6-svg,qt6-tools,qt6-translations,opus,openssl}
        ;;
    --tests) TESTS=ON ;;
    *) echo "unknown option $arg" >&2; exit 1 ;;
    esac
done

# Tests on Windows run offline, so pjsua (the remote party) isn't needed.
PJSIP_LIBS_ONLY=1 "$ROOT/scripts/build-pjsip.sh"

BUILD="$ROOT/build-win"
cmake -G Ninja -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DKKSIP_BUILD_TESTS=$TESTS
cmake --build "$BUILD"

VERSION="$(sed -n 's/^project(kk-sip VERSION \([0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt")"
STAGE="$ROOT/dist/stage"
rm -rf "$STAGE"
mkdir -p "$STAGE"
cp "$BUILD/kk-sip.exe" "$STAGE/"
cp "$ROOT/LICENSE" "$ROOT/THIRD-PARTY.md" "$STAGE/"

WINDEPLOYQT="$(command -v windeployqt6 || command -v windeployqt-qt6 || command -v windeployqt)"
"$WINDEPLOYQT" --release --no-compiler-runtime --no-system-d3d-compiler --no-opengl-sw \
    --translations en,ru,uk --skip-plugin-types networkinformation,generic \
    "$STAGE/kk-sip.exe"

# windeployqt copies Qt only; pull in everything else from /ucrt64/bin (opus, OpenSSL,
# ICU, zlib, MinGW runtime...) until nothing new turns up. System DLLs aren't there.
while :; do
    added=0
    for name in $(find "$STAGE" \( -name '*.exe' -o -name '*.dll' \) -exec objdump -p {} + \
                      | sed -n 's/^\s*DLL Name: //p' | sort -u); do
        if [[ ! -e "$STAGE/$name" && -e "/ucrt64/bin/$name" ]]; then
            cp "/ucrt64/bin/$name" "$STAGE/"
            added=1
        fi
    done
    [[ $added == 0 ]] && break
done

mkdir -p "$ROOT/dist"
ZIP="$ROOT/dist/kk-sip-$VERSION-windows-x64.zip"
rm -f "$ZIP"
(cd "$ROOT/dist" && cp -r stage "kk-sip-$VERSION" && zip -qr "$ZIP" "kk-sip-$VERSION" && rm -rf "kk-sip-$VERSION")
echo "Staged in $STAGE ($(du -sh "$STAGE" | cut -f1)), portable: $ZIP"
