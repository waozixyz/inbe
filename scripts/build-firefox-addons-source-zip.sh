#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT_DIR"

OUT=${1:-build/dist/inbe-firefox-addons-source.zip}
case $OUT in
    /*) OUT_ABS=$OUT ;;
    *) OUT_ABS=$ROOT_DIR/$OUT ;;
esac
STAGE_ROOT=${TMPDIR:-/tmp}/inbe-firefox-addons-source-$$
STAGE_DIR=$STAGE_ROOT/inbe-firefox-addons-source

cleanup() {
    rm -rf "$STAGE_ROOT"
}
trap cleanup EXIT HUP INT TERM

mkdir -p "$STAGE_DIR" "$(dirname -- "$OUT_ABS")"

copy_path() {
    src=$1
    if [ -e "$src" ]; then
        mkdir -p "$STAGE_DIR/$(dirname -- "$src")"
        cp -R "$src" "$STAGE_DIR/$src"
    fi
}

for path in \
    .gitignore \
    .gitmodules \
    .github \
    Makefile \
    README.md \
    manifest.json \
    assets \
    locales \
    packaging \
    scripts \
    site-icons \
    src \
    tests \
    unpackaged_assets \
    vendor \
    web-assets
 do
    copy_path "$path"
 done

rm -f "$OUT_ABS"
(
    cd "$STAGE_ROOT"
    zip -q -9 -r "$OUT_ABS" inbe-firefox-addons-source \
        -x '*/.git' \
           '*/.git/*' \
           '*/.claude' \
           '*/.claude/*' \
           '*/.codex' \
           '*/.codex/*' \
           '*/.ccache/*' \
           '*/.gradle/*' \
           '*/node_modules/*' \
           '*/build/*' \
           '*/vendor-builds/*' \
           '*/unpackaged_assets/audio/*' \
           '*/web-assets/dl/*' \
           '*/vendor/sqlite/art/*' \
           '*/vendor/sqlite/doc/*' \
           '*/vendor/sqlite/mptest/*' \
           '*/vendor/sqlite/test/*' \
           '*/vendor/kryon/docs/site/*' \
           '*/vendor/kryon/fonts/noto/*' \
           '*/vendor/kryon/vendor/liboqs/docs/*' \
           '*/vendor/kryon/vendor/liboqs/tests/*' \
           '*/vendor/kryon/vendor/raylib/examples/*' \
           '*/vendor/kryon/vendor/raylib/logo/*' \
           '*/vendor/kryon/vendor/raylib/projects/*' \
           '*/tmp/*'
)

if [ ! -f "$OUT_ABS" ]; then
    printf 'ERROR: expected source zip not found: %s\n' "$OUT_ABS" >&2
    exit 1
fi

printf 'Firefox add-on source zip created: %s\n' "$OUT_ABS"
