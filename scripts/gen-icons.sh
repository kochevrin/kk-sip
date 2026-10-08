#!/usr/bin/env bash
# Renders resources/icons/kk-sip.svg into the PNG sizes packages and docs need.
# Needs rsvg-convert (librsvg) and icotool (icoutils). Commit the results; builds
# don't run this.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SVG="$ROOT/resources/icons/kk-sip.svg"
for size in 16 22 24 32 48 64 128 256 512; do
    mkdir -p "$ROOT/resources/icons/hicolor/${size}x${size}"
    rsvg-convert -w "$size" -h "$size" "$SVG" -o "$ROOT/resources/icons/hicolor/${size}x${size}/kk-sip.png"
done
# Windows .exe icon; 256 stored as PNG like Windows does.
H="$ROOT/resources/icons/hicolor"
icotool -c -o "$ROOT/resources/icons/kk-sip.ico" \
    "$H"/{16x16,24x24,32x32,48x48,64x64,128x128}/kk-sip.png -r "$H/256x256/kk-sip.png"
rsvg-convert -w 192 -h 192 "$SVG" -o "$ROOT/docs/logo.png"
echo "icons written"
