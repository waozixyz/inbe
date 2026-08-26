#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UNAME_S="$(uname -s)"
UNAME_M="$(uname -m)"
case "$UNAME_S" in
  FreeBSD) PLATFORM="freebsd" ;;
  Linux) PLATFORM="linux" ;;
  *) PLATFORM="$(printf '%s' "$UNAME_S" | tr '[:upper:]' '[:lower:]')" ;;
esac
case "$UNAME_M" in
  amd64) ARCH="x86_64" ;;
  *) ARCH="$UNAME_M" ;;
esac

BIN="${1:-"$ROOT_DIR/build/bin/$PLATFORM/inbe-$PLATFORM-$ARCH"}"
OUT_DIR="${TERMI_SCREENSHOT_OUT_DIR:-"$ROOT_DIR/build/termi-screenshots"}"
SCENE="${TERMI_SCREENSHOT_SCENE:-wim_hof_session}"
TITLE="inbe-termi-shot-$$"
DATA_ROOT="$OUT_DIR/data-$SCENE"
XWD_1="$OUT_DIR/$SCENE-1.xwd"
XWD_2="$OUT_DIR/$SCENE-2.xwd"
PNG_1="$OUT_DIR/$SCENE-1.png"
PNG_2="$OUT_DIR/$SCENE-2.png"

for tool in xvfb-run xterm xdotool xwd convert python3; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "termi screenshot test: missing required tool: $tool" >&2
    exit 1
  fi
done

if [[ ! -x "$BIN" ]]; then
  echo "termi screenshot test: binary is missing or not executable: $BIN" >&2
  echo "Run: make KRYON_BACKEND=termi native" >&2
  exit 1
fi

mkdir -p "$OUT_DIR"
rm -rf "$DATA_ROOT"
mkdir -p "$DATA_ROOT"
rm -f "$XWD_1" "$XWD_2" "$PNG_1" "$PNG_2"

xvfb-run -a bash -s -- "$BIN" "$TITLE" "$DATA_ROOT" "$XWD_1" "$XWD_2" "$SCENE" <<'CAPTURE'
set -euo pipefail

bin="$1"
title="$2"
data_root="$3"
xwd_1="$4"
xwd_2="$5"
scene="$6"

INBE_SHOT_WINDOW=1 \
INBE_NO_TRAY=1 \
INBE_DATA_ROOT="$data_root" \
TERMI_SIXEL=0 \
TERMI_COLS=120 \
TERMI_ROWS=42 \
xterm -geometry 120x42 -fa Monospace -fs 10 -title "$title" \
  -e "$bin" \
  --screenshot /tmp/inbe-termi-unused.png \
  --screenshot-scene "$scene" \
  --screenshot-width 960 \
  --screenshot-height 672 \
  --screenshot-theme -1 \
  --screenshot-dark 0 \
  --screenshot-style 2 &
pid=$!
trap 'kill "$pid" 2>/dev/null || true' EXIT INT TERM

wid=""
for _ in $(seq 1 50); do
  wid="$(xdotool search --name "$title" 2>/dev/null | head -n 1 || true)"
  if [[ -n "$wid" ]]; then
    break
  fi
  sleep 0.1
done

if [[ -z "$wid" ]]; then
  echo "termi screenshot test: xterm window was not created" >&2
  exit 1
fi

sleep "${TERMI_SCREENSHOT_WARMUP:-2}"
xwd -silent -id "$wid" -out "$xwd_1"
sleep "${TERMI_SCREENSHOT_FRAME_GAP:-1}"
xwd -silent -id "$wid" -out "$xwd_2"
CAPTURE

convert "$XWD_1" "$PNG_1"
convert "$XWD_2" "$PNG_2"

python3 - "$PNG_1" "$PNG_2" <<'PY'
import subprocess
import sys


def load_rgb(path):
    data = subprocess.check_output(["convert", path, "ppm:-"])
    i = 0

    def token():
        nonlocal i
        while i < len(data) and data[i] in b" \t\r\n":
            i += 1
        if i < len(data) and data[i] == ord("#"):
            while i < len(data) and data[i] not in b"\r\n":
                i += 1
            return token()
        start = i
        while i < len(data) and data[i] not in b" \t\r\n":
            i += 1
        return data[start:i]

    magic = token()
    if magic != b"P6":
        raise SystemExit(f"{path}: expected P6 PPM, got {magic!r}")
    width = int(token())
    height = int(token())
    max_value = int(token())
    if i < len(data) and data[i] in b" \t\r\n":
        i += 1
    if max_value != 255:
        raise SystemExit(f"{path}: unsupported max value {max_value}")
    pixels = data[i:]
    if len(pixels) != width * height * 3:
        raise SystemExit(f"{path}: malformed pixel data")
    return width, height, pixels


def stats(path):
    width, height, pixels = load_rgb(path)
    count = width * height
    unique = set()
    black = 0
    bright = 0
    colorful = 0
    for i in range(0, len(pixels), 3):
        r, g, b = pixels[i], pixels[i + 1], pixels[i + 2]
        unique.add((r, g, b))
        if r < 8 and g < 8 and b < 8:
            black += 1
        if max(r, g, b) > 170:
            bright += 1
        if max(r, g, b) - min(r, g, b) > 50 and max(r, g, b) > 120:
            colorful += 1
    return {
        "path": path,
        "width": width,
        "height": height,
        "pixels": pixels,
        "unique": len(unique),
        "black_pct": black / count,
        "bright_pct": bright / count,
        "colorful_pct": colorful / count,
    }


first = stats(sys.argv[1])
second = stats(sys.argv[2])

for item in (first, second):
    if item["width"] < 600 or item["height"] < 400:
        raise SystemExit(f"{item['path']}: screenshot is too small")
    if item["unique"] < 16:
        raise SystemExit(f"{item['path']}: too few colors ({item['unique']})")
    if item["black_pct"] > 0.12:
        raise SystemExit(
            f"{item['path']}: too much pure black ({item['black_pct']:.1%})"
        )
    if item["bright_pct"] < 0.02:
        raise SystemExit(
            f"{item['path']}: missing bright foreground content "
            f"({item['bright_pct']:.1%})"
        )
    if item["colorful_pct"] < 0.01:
        raise SystemExit(
            f"{item['path']}: missing colorful practice content "
            f"({item['colorful_pct']:.1%})"
        )

changed = sum(a != b for a, b in zip(first["pixels"], second["pixels"])) // 3
changed_pct = changed / (first["width"] * first["height"])

print(
    "termi screenshot ok: "
    f"{first['width']}x{first['height']}, "
    f"colors={first['unique']}, "
    f"black={first['black_pct']:.1%}, "
    f"changed={changed_pct:.1%}"
)
PY
