#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT_DIR"

MAKE_CMD=${MAKE:-}
if [ -z "$MAKE_CMD" ]; then
    if command -v gmake >/dev/null 2>&1; then
        MAKE_CMD=gmake
    else
        MAKE_CMD=make
    fi
fi

if [ -z "${WEB_CACHE_BUSTER:-}" ]; then
    if command -v git >/dev/null 2>&1 && git rev-parse --short HEAD >/dev/null 2>&1; then
        WEB_CACHE_BUSTER=$(git rev-parse --short HEAD)
    else
        WEB_CACHE_BUSTER=$(date +%s)
    fi
    export WEB_CACHE_BUSTER
fi

print_version() {
    label=$1
    shift
    if command -v "$1" >/dev/null 2>&1; then
        printf '%s: ' "$label"
        "$@" 2>&1 | sed -n '/./{p;q;}'
    else
        printf '%s: missing (%s)\n' "$label" "$1"
        return 1
    fi
}

printf '%s\n' 'Inner Breeze Firefox add-on source build'
printf 'Repository: %s\n' "$ROOT_DIR"
printf 'Make: %s\n' "$MAKE_CMD"
printf 'WEB_CACHE_BUSTER: %s\n' "$WEB_CACHE_BUSTER"
if [ -n "${ADDONS_LINTER:-}" ]; then
    printf 'ADDONS_LINTER: %s\n' "$ADDONS_LINTER"
else
    printf '%s\n' 'ADDONS_LINTER: Makefile default'
fi

print_version 'GNU Make' "$MAKE_CMD" --version
print_version 'Node.js' node --version
print_version 'npm' npm --version
print_version 'Emscripten' emcc --version
print_version 'CMake' cmake --version
print_version 'zip' zip --version
print_version 'perl' perl --version

"$MAKE_CMD" package-unpackaged-assets
"$MAKE_CMD" firefox-addons-lint

target=build/dist/inbe-firefox-addons.zip
if [ ! -f "$target" ]; then
    printf 'ERROR: expected output not found: %s\n' "$target" >&2
    exit 1
fi

printf 'Firefox add-on zip created: %s\n' "$target"
