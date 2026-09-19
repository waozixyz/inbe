#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
main_host="$root/src/platform/app_host.kry"

if grep -Eq 'ConfigureFramePacing\([[:space:]]*5[[:space:]]*,[[:space:]]*60[[:space:]]*\)' "$main_host"; then
    echo "desktop frame pacing must not idle at 5 FPS"
    exit 1
fi

if ! grep -Eq 'ConfigureFramePacing\([[:space:]]*30[[:space:]]*,[[:space:]]*60[[:space:]]*\)' "$main_host"; then
    echo "desktop frame pacing should idle at 30 FPS and stay active at 60 FPS"
    exit 1
fi

echo "desktop frame pacing policy test passed"
