#!/usr/bin/env bash
set -euo pipefail

serial="${MOTO_SERIAL:-ZE2223BQZT}"
out="${1:-/tmp/moto-screen.png}"

mkdir -p "$(dirname "$out")"
adb -s "$serial" exec-out screencap -p > "$out"
printf '%s\n' "$out"
