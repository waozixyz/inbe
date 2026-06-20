#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${1:-"$ROOT_DIR/build/bin/linux/inbe-linux-$(uname -m)"}"
OUT_DIR="${SCREENSHOT_OUT_DIR:-"$ROOT_DIR/tmp"}"
DATA_BASE="$OUT_DIR/screenshot-data"

SCENES=(
  "home:01-home:0:0"
  "calendar_meditation:02-calendar-meditation:1:0"
  "habits_stats:03-habits-statistics:2:1"
  "theme_selection:04-theme-selection:0:0"
  "cobalt_dark:05-cobalt-dark:11:1"
  "manual_whm:06-wim-hof-manual:3:0"
  "tutorial_whm_step2:07-wim-hof-step-2:7:1"
  "meditation_tutorial:08-meditation-tutorial:4:0"
  "home:09-home-mono-light:9:0"
  "habits_stats:10-statistics-mint-light:10:0"
  "manual_whm:11-wim-hof-ocean-dark:1:1"
  "meditation_tutorial:12-meditation-dawn-dark:6:1"
)

BUCKETS=(
  "phone:1080:1920"
  "tablet-7:1920:1080"
  "tablet-10:2560:1440"
  "chromebook:2560:1440"
)

if [[ ! -x "$BIN" ]]; then
  echo "Screenshot binary is missing or not executable: $BIN" >&2
  echo "Run: make native" >&2
  exit 1
fi
if ! command -v xvfb-run >/dev/null 2>&1; then
  echo "xvfb-run is required so screenshots are not clamped by your monitor size." >&2
  echo "Enter the flake shell with: nix develop" >&2
  exit 1
fi

mkdir -p "$OUT_DIR"
rm -rf "$OUT_DIR/phone" "$OUT_DIR/tablet-7" "$OUT_DIR/tablet-10" \
       "$OUT_DIR/chromebook" "$DATA_BASE"
mkdir -p "$DATA_BASE"

run_app() {
  local width="$1"
  local height="$2"
  local data_root="$3"
  local scene="$4"
  local output="$5"
  local theme="$6"
  local dark="$7"

  INBE_DATA_ROOT="$data_root" xvfb-run -a -s "-screen 0 ${width}x${height}x24" \
    "$BIN" \
    --screenshot "$output" \
    --screenshot-scene "$scene" \
    --screenshot-width "$width" \
    --screenshot-height "$height" \
    --screenshot-theme "$theme" \
    --screenshot-dark "$dark"
}

count=0
for bucket in "${BUCKETS[@]}"; do
  IFS=: read -r bucket_name width height <<<"$bucket"
  bucket_dir="$OUT_DIR/$bucket_name"
  mkdir -p "$bucket_dir"

  for scene_def in "${SCENES[@]}"; do
    IFS=: read -r scene slug theme dark <<<"$scene_def"
    output="$bucket_dir/${slug}-${width}x${height}.png"
    data_root="$DATA_BASE/${bucket_name}-${scene}"
    mkdir -p "$data_root"
    echo "Generating $bucket_name/$slug ${width}x${height}"
    run_app "$width" "$height" "$data_root" "$scene" "$output" "$theme" "$dark"
    if [[ ! -s "$output" ]]; then
      echo "Screenshot was not created: $output" >&2
      exit 1
    fi
    actual_size="$(file "$output")"
    if [[ "$actual_size" != *"${width} x ${height}"* && "$actual_size" != *"${width}x${height}"* ]]; then
      echo "Screenshot has wrong dimensions: $output" >&2
      echo "Expected ${width}x${height}; got: $actual_size" >&2
      exit 1
    fi
    bytes="$(stat -c %s "$output" 2>/dev/null || wc -c <"$output")"
    if [[ "$bytes" -gt 8388608 ]]; then
      echo "Warning: $output is larger than 8 MB" >&2
    fi
    count=$((count + 1))
  done
done

echo "Wrote $count screenshots to $OUT_DIR"
