#!/usr/bin/env bash
# Renders resources/icons/kk-sip.svg into the PNG sizes packages and docs need.
# Needs rsvg-convert (librsvg). Commit the results; builds don't run this.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SVG="$ROOT/resources/icons/kk-sip.svg"
for size in 16 22 24 32 48 64 128 256 512; do
    mkdir -p "$ROOT/resources/icons/hicolor/${size}x${size}"
    rsvg-convert -w "$size" -h "$size" "$SVG" -o "$ROOT/resources/icons/hicolor/${size}x${size}/kk-sip.png"
done
rsvg-convert -w 192 -h 192 "$SVG" -o "$ROOT/docs/logo.png"
echo "icons written"
