#!/usr/bin/env bash
# Runs the end-to-end smoke test against a throwaway Asterisk in Docker. Pinned to 22.10.1:
# 22.11.0 and 23.5.0 leave the caller (<remote>) out of the dialog-event NOTIFY of a
# ringing extension, which the busy lamp test checks.
# Audio goes to PJSIP's null device, so nothing is played on the speakers.
#
#   scripts/smoke-test.sh                 # run tests
#   KKSIP_SCREENSHOTS=shots scripts/smoke-test.sh   # also save UI screenshots
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CONTAINER=kksip-test-asterisk

"$ROOT/scripts/build-pjsip.sh"
cmake -S "$ROOT" -B "$ROOT/build" -DKKSIP_BUILD_TESTS=ON
cmake --build "$ROOT/build" -j"$(nproc)"

docker rm -f "$CONTAINER" >/dev/null 2>&1 || true
docker run -d --name "$CONTAINER" --network host \
    -v "$ROOT/tests/asterisk/pjsip.conf:/etc/asterisk/pjsip.conf:ro" \
    -v "$ROOT/tests/asterisk/extensions.conf:/etc/asterisk/extensions.conf:ro" \
    andrius/asterisk:22.10.1_debian-trixie >/dev/null
trap 'docker rm -f "$CONTAINER" >/dev/null 2>&1 || true' EXIT

for _ in $(seq 30); do
    docker exec "$CONTAINER" asterisk -rx "pjsip show transports" 2>/dev/null | grep -q transport-udp && break
    sleep 1
done

QT_QPA_PLATFORM=offscreen \
KKSIP_PJSUA="$(ls "$ROOT"/.deps/src/pjproject-*/pjsip-apps/bin/pjsua-* | head -1)" \
    "$ROOT/build/kk-sip-smoke" "$@"
