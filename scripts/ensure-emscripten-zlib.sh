#!/usr/bin/env bash
set -euo pipefail

embuilder=${1:-embuilder}
for attempt in 1 2 3; do
    if "$embuilder" build zlib; then
        exit 0
    fi
    if [ "$attempt" -eq 3 ]; then
        echo "Emscripten zlib port could not be fetched after 3 attempts" >&2
        exit 1
    fi
    sleep $((attempt * 10))
done
