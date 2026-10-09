#!/usr/bin/env bash
# Builds kk-sip for macOS on Apple Silicon with Homebrew's Qt, opus and OpenSSL and
# packs a self-contained kk-sip.app into dist/kk-sip-<ver>-macos-arm64.dmg.
# The app is signed ad hoc only (no Developer ID), see README for the first launch.
#
#   packaging/macos/build-macos.sh [--install-deps] [--tests]
#
#   --install-deps  brew install the tools and libraries first
#   --tests         also build kk-sip-smoke (offline part runs without a PBX)
set -euo pipefail

if [[ "$(uname -s)" != Darwin ]]; then
    echo "Run this on macOS." >&2
    exit 1
fi

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TESTS=OFF
for arg in "$@"; do
    case "$arg" in
    # Only the Qt modules kk-sip uses: the "qt" meta package pulls in every module,
    # and macdeployqt then ships their plugins too.
    --install-deps) brew install cmake ninja pkgconf qtbase qtsvg qttools qttranslations opus openssl@3 ;;
    --tests) TESTS=ON ;;
    *) echo "unknown option $arg" >&2; exit 1 ;;
    esac
done

BREW="$(brew --prefix)"
# PJSIP's configure and the linker look for opus and OpenSSL in Homebrew's prefix.
export CPATH="$BREW/include${CPATH:+:$CPATH}"
export LIBRARY_PATH="$BREW/lib${LIBRARY_PATH:+:$LIBRARY_PATH}"
# Homebrew bottles from the macos-14 runner need Sonoma anyway (see Info.plist.in).
export MACOSX_DEPLOYMENT_TARGET=14.0

# Tests on macOS run offline, so pjsua (the remote party) isn't needed.
PJSIP_LIBS_ONLY=1 "$ROOT/scripts/build-pjsip.sh"

BUILD="$ROOT/build-mac"
cmake -G Ninja -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DKKSIP_BUILD_TESTS=$TESTS \
    -DCMAKE_PREFIX_PATH="$BREW"
cmake --build "$BUILD"

VERSION="$(sed -n 's/^project(kk-sip VERSION \([0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt")"
STAGE="$ROOT/dist/stage-mac"
APP="$STAGE/kk-sip.app"
rm -rf "$STAGE"
mkdir -p "$STAGE"
cp -R "$BUILD/kk-sip.app" "$STAGE/"
cp "$ROOT/LICENSE" "$ROOT/THIRD-PARTY.md" "$APP/Contents/Resources/"

# Copies Qt frameworks, plugins and the Homebrew dylibs (opus, OpenSSL...) into the bundle.
MACDEPLOYQT="$(command -v macdeployqt6 || command -v macdeployqt)"
# Each Qt module and library is its own Homebrew keg; macdeployqt resolves @rpath
# only through the paths in the binary itself plus these.
LIBPATHS=()
for keg in qtbase qtsvg brotli; do
    LIBPATHS+=("-libpath=$(brew --prefix "$keg")/lib")
done
"$MACDEPLOYQT" "$APP" "${LIBPATHS[@]}"
# Only SQLite is used; the other SQL drivers would drag in client libraries.
find "$APP/Contents/PlugIns/sqldrivers" -name '*.dylib' ! -name 'libqsqlite.dylib' -delete

# Qt's own strings (OK, Cancel...) in Ukrainian and Russian. main.cpp asks
# QLibraryInfo for them, and qt.conf (written by macdeployqt) points it here.
QT_TRANSLATIONS="$(qtpaths6 --query QT_INSTALL_TRANSLATIONS 2>/dev/null || qtpaths --query QT_INSTALL_TRANSLATIONS)"
mkdir -p "$APP/Contents/Resources/translations"
for lang in ru uk; do
    cp "$QT_TRANSLATIONS"/qtbase_$lang.qm "$APP/Contents/Resources/translations/"
done
printf "\nTranslations = Resources/translations\n" >> "$APP/Contents/Resources/qt.conf"

# Everything the bundle loads must be inside it or part of macOS.
missing=0
while IFS= read -r bin; do
    self="$(otool -D "$bin" | tail -n +2)" # a dylib's own install name, listed first by otool -L
    while IFS= read -r dep; do
        [[ "$dep" == "$self" ]] && continue
        case "$dep" in
        /System/* | /usr/lib/*) ;;
        @rpath/* | @executable_path/../Frameworks/*)
            [[ -e "$APP/Contents/Frameworks/${dep#*/Frameworks/}" || -e "$APP/Contents/Frameworks/${dep#@rpath/}" ]] \
                || { echo "missing $dep (needed by ${bin#$APP/})"; missing=1; } ;;
        @loader_path/*) ;;
        *) echo "outside the bundle: $dep (needed by ${bin#$APP/})"; missing=1 ;;
        esac
    done < <(otool -L "$bin" | tail -n +2 | awk '{print $1}')
done < <(find "$APP/Contents" -type f \( -name '*.dylib' -o -perm -u+x \) -exec sh -c 'file -b "$1" | grep -q Mach-O' _ {} \; -print)
[[ $missing == 0 ]] || exit 1

# Apple Silicon refuses to run unsigned code; an ad hoc signature is enough for that.
codesign --force --deep --sign - "$APP"
codesign --verify --deep --strict "$APP"
# Loads every linked framework from the bundle.
"$APP/Contents/MacOS/kk-sip" --version
du -sh "$APP"/Contents/Frameworks/* "$APP"/Contents/PlugIns/*/* | sort -h | tail -n 15

# Drag-to-Applications disk image.
ln -s /Applications "$STAGE/Applications"
DMG="$ROOT/dist/kk-sip-$VERSION-macos-arm64.dmg"
rm -f "$DMG"
# hdiutil on CI runners sometimes fails with "Resource busy"; a retry helps.
for attempt in 1 2 3; do
    hdiutil create -volname "kk-sip $VERSION" -srcfolder "$STAGE" -fs HFS+ -format UDZO -ov "$DMG" && break
    [[ $attempt == 3 ]] && exit 1
    sleep 5
done
echo "App: $APP ($(du -sh "$APP" | cut -f1)), disk image: $DMG ($(du -h "$DMG" | cut -f1))"
