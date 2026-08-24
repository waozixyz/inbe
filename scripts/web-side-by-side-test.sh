#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

BUILD=1
SMOKE=1
SERVE=1
OPEN=0
PORT="${INBE_WEB_SIDE_BY_SIDE_PORT:-0}"
BROWSER="${WEB_SMOKE_BROWSER:-auto}"
STAMP="$(date +%Y%m%d-%H%M%S)"
LOG_DIR="${INBE_WEB_SIDE_BY_SIDE_LOG_DIR:-/tmp/inbe-web-side-by-side-$STAMP}"
WORK_DIR="${INBE_WEB_SIDE_BY_SIDE_WORK_DIR:-/tmp/inbe-web-side-by-side-view-$STAMP}"
export EM_CACHE="${EM_CACHE:-/tmp/inbe-em-cache}"
mkdir -p "$EM_CACHE"

usage() {
    cat <<EOF
Usage: $0 [options]

Build and test both web renderers, then serve a side-by-side comparison page.

Options:
  --no-build      Skip rebuilding build/dist/web and build/dist/web-canvas.
  --no-smoke      Skip automated smoke tests.
  --no-serve      Exit after build/smoke checks.
  --open          Open the side-by-side page with xdg-open after serving starts.
  --port PORT     Serve on PORT. Default chooses a free local port.
  --browser CMD   Browser command for smoke tests. Default: WEB_SMOKE_BROWSER or auto.
  -h, --help      Show this help.

Environment:
  WEB_SMOKE_BROWSER                  Browser for smoke tests.
  INBE_WEB_SIDE_BY_SIDE_PORT         Default port when --port is omitted.
  INBE_WEB_SIDE_BY_SIDE_LOG_DIR      Directory for command logs.
  INBE_WEB_SIDE_BY_SIDE_WORK_DIR     Temporary serving root.
  EM_CACHE                           Emscripten cache. Default: /tmp/inbe-em-cache.
EOF
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --no-build)
            BUILD=0
            ;;
        --no-smoke)
            SMOKE=0
            ;;
        --no-serve)
            SERVE=0
            ;;
        --open)
            OPEN=1
            ;;
        --port)
            shift
            if [ "$#" -eq 0 ]; then
                echo "Missing value for --port" >&2
                exit 2
            fi
            PORT="$1"
            ;;
        --browser)
            shift
            if [ "$#" -eq 0 ]; then
                echo "Missing value for --browser" >&2
                exit 2
            fi
            BROWSER="$1"
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
    shift
done

run_step() {
    local name="$1"
    local label="$2"
    shift 2
    local log="$LOG_DIR/$name.log"

    mkdir -p "$LOG_DIR"
    printf '==> %s\n' "$label"
    if "$@" >"$log" 2>&1; then
        printf '    PASS (%s)\n' "$log"
        return 0
    fi

    printf '    FAIL (%s)\n' "$log" >&2
    tail -120 "$log" >&2 || true
    exit 1
}

choose_port() {
    if [ "$PORT" != "0" ]; then
        printf '%s\n' "$PORT"
        return 0
    fi
    python3 - <<'PY'
import socket

sock = socket.socket()
sock.bind(("127.0.0.1", 0))
print(sock.getsockname()[1])
sock.close()
PY
}

write_side_by_side_page() {
    mkdir -p "$WORK_DIR"
    ln -sfn "$ROOT/build/dist/web" "$WORK_DIR/raylib"
    ln -sfn "$ROOT/build/dist/web-canvas" "$WORK_DIR/canvas"
    ln -sfn "$ROOT/build/dist/web/site-icons" "$WORK_DIR/site-icons"
    ln -sfn "$ROOT/build/dist/web/web-assets" "$WORK_DIR/web-assets"
    cat > "$WORK_DIR/index.html" <<'HTML'
<!doctype html>
<html lang="en">
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Inner Breeze Web Renderer Compare</title>
    <style>
      html,
      body {
        height: 100%;
        margin: 0;
        background: #111;
        color: #f7fbf9;
        font: 13px/1.4 ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
      }

      .compare {
        min-height: 100%;
        display: grid;
        grid-template-columns: 1fr 1fr;
      }

      .pane {
        min-width: 0;
        display: grid;
        grid-template-rows: auto 1fr;
        border-left: 1px solid rgba(255, 255, 255, 0.18);
      }

      .pane:first-child {
        border-left: 0;
      }

      .bar {
        min-height: 34px;
        display: flex;
        align-items: center;
        gap: 10px;
        padding: 0 12px;
        background: #20242a;
        border-bottom: 1px solid rgba(255, 255, 255, 0.18);
        box-sizing: border-box;
      }

      .label {
        font-weight: 700;
      }

      .path {
        color: #aab8b4;
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
      }

      iframe {
        width: 100%;
        height: 100%;
        border: 0;
        background: #17172a;
      }

      @media (max-width: 900px) {
        .compare {
          grid-template-columns: 1fr;
          grid-template-rows: 1fr 1fr;
        }

        .pane {
          border-left: 0;
          border-top: 1px solid rgba(255, 255, 255, 0.18);
        }

        .pane:first-child {
          border-top: 0;
        }
      }
    </style>
  </head>
  <body>
    <main class="compare">
      <section class="pane">
        <div class="bar">
          <span class="label">Raylib / WebGL</span>
          <span class="path">/raylib/index.html</span>
        </div>
        <iframe src="/raylib/index.html" title="Inner Breeze Raylib WebGL"></iframe>
      </section>
      <section class="pane">
        <div class="bar">
          <span class="label">Canvas2D</span>
          <span class="path">/canvas/index.html</span>
        </div>
        <iframe src="/canvas/index.html" title="Inner Breeze Canvas2D"></iframe>
      </section>
    </main>
  </body>
</html>
HTML
}

cleanup() {
    if [ "$SERVE" -eq 0 ]; then
        rm -rf "$WORK_DIR"
    fi
}
trap cleanup EXIT

if [ "$BUILD" -eq 1 ]; then
    run_step build "Build raylib/WebGL and Canvas web outputs" make build/dist/web/index.html
fi

if [ ! -f build/dist/web/index.html ] || [ ! -f build/dist/web/index.js ] || [ ! -f build/dist/web/index.wasm ]; then
    echo "Missing raylib/WebGL web build in build/dist/web" >&2
    exit 1
fi

if [ ! -f build/dist/web-canvas/index.html ] || [ ! -f build/dist/web-canvas/index.js ] || [ ! -f build/dist/web-canvas/index.wasm ]; then
    echo "Missing Canvas web build in build/dist/web-canvas" >&2
    exit 1
fi

if [ "$SMOKE" -eq 1 ]; then
    run_step smoke-raylib "Smoke test raylib/WebGL renderer" \
        env WEB_SMOKE_RENDERER=raylib WEB_SMOKE_BROWSER="$BROWSER" node scripts/web-smoke-test.mjs build/dist/web
    run_step smoke-canvas "Smoke test Canvas2D renderer" \
        env WEB_SMOKE_RENDERER=canvas WEB_SMOKE_BROWSER="$BROWSER" node scripts/web-smoke-test.mjs build/dist/web-canvas
fi

if [ "$SERVE" -eq 0 ]; then
    printf 'Logs: %s\n' "$LOG_DIR"
    exit 0
fi

write_side_by_side_page
PORT="$(choose_port)"
URL="http://127.0.0.1:$PORT/"

printf '\nSide-by-side page: %s\n' "$URL"
printf 'Logs: %s\n' "$LOG_DIR"
printf 'Serving root: %s\n' "$WORK_DIR"
printf 'Press Ctrl-C to stop.\n\n'

if [ "$OPEN" -eq 1 ]; then
    xdg-open "$URL" >/dev/null 2>&1 || true
fi

exec python3 -m http.server "$PORT" --bind 127.0.0.1 --directory "$WORK_DIR"
