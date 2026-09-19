#!/bin/sh
# End-to-end desktop window tests for Inner Breeze.
#
# Drives the real binary on a private Xvfb display with a window manager,
# using xdotool, exactly like a user: clicks the title-bar close button and
# answers the close prompt.
#
# Covered cases (the "closing windows in different inbe modes" matrix):
#   1. startup        main window maps on a fresh profile
#   2. close-keep     close action = keep running -> window hides, app lives
#   3. close-ask      close action = ask -> prompt modal -> Quit -> app exits
#   4. close-ask-keep close action = ask -> prompt -> Keep running -> hidden
#
# Requirements: xvfb-run/Xvfb, xfwm4, xdotool, xwininfo (x11-utils), sqlite3.
# The user's session and data are never touched. Break HUD dragging needs a
# separate tray-ready harness; this no-tray close-window test intentionally does
# not assert HUD behavior.
#
# Usage: scripts/test-desktop-windows.sh [path-to-binary]

set -u

BIN=${1:-./build/bin/linux/inbe-linux-x86_64}
GEOMETRY="1280x800x24"

[ -x "$BIN" ] || { echo "FAIL: binary not found: $BIN"; exit 1; }
for tool in xvfb-run Xvfb xfwm4 xdotool xwininfo sqlite3; do
    command -v "$tool" >/dev/null 2>&1 || {
        echo "FAIL: missing tool: $tool (install xvfb, xfwm4, xdotool, x11-utils, sqlite3)"
        exit 1
    }
done

if [ -z "${INBE_DESKTOP_WINDOW_TEST_XVFB:-}" ]; then
    exec xvfb-run -a -s "-screen 0 $GEOMETRY" \
        env INBE_DESKTOP_WINDOW_TEST_XVFB=1 "$0" "$@"
fi

DISPLAY_TEST=${DISPLAY:?xvfb-run did not set DISPLAY}
WORK=$(mktemp -d /tmp/inbe-winmode.XXXXXX)
PASS=0
FAIL=0
APP_PID=""

cleanup() {
    [ -n "$APP_PID" ] && kill "$APP_PID" 2>/dev/null
    if [ -n "${XVFB_PID:-}" ]; then kill "$XVFB_PID" 2>/dev/null; fi
    if [ -n "${WM_PID:-}" ]; then kill "$WM_PID" 2>/dev/null; fi
    sleep 0.5
    rm -rf "$WORK"
}
trap cleanup EXIT INT TERM

say() { printf '%s\n' "$*"; }
ok()  { PASS=$((PASS + 1)); say "PASS: $1"; }
bad() { FAIL=$((FAIL + 1)); say "FAIL: $1"; }

XD() { DISPLAY="$DISPLAY_TEST" xdotool "$@" 2>/dev/null; }
XW() { DISPLAY="$DISPLAY_TEST" xwininfo "$@" 2>/dev/null; }

# Main (client) window id: named, wider than 500px.
main_window() {
    for w in $(XD search --name "Inner Breeze"); do
        width=$(XD getwindowgeometry --shell "$w" | grep '^WIDTH' | cut -d= -f2)
        [ "${width:-0}" -gt 500 ] && { echo "$w"; return; }
    done
}

# Title-bar close button position, derived from the WM frame geometry.
close_button() {
    frame=$(XW -root -children | grep -E "904x754\+[0-9]+\+" |
            grep -oE '0x[0-9a-f]+' | head -1)
    [ -n "$frame" ] || return 1
    fx=$(XW -id "$frame" | grep 'Absolute upper-left X' | grep -oE '[0-9-]+$')
    fy=$(XW -id "$frame" | grep 'Absolute upper-left Y' | grep -oE '[0-9-]+$')
    fw=$(XW -id "$frame" | grep '^  Width' | grep -oE '[0-9]+$')
    [ -n "$fw" ] || return 1
    echo "$((fx + fw - 24)) $((fy + 13))"
}

launch_app() { # $1 = root dir, $2 = log name
    [ -n "$APP_PID" ] && { kill "$APP_PID" 2>/dev/null; sleep 1; }
    rm -f "$WORK/$2"
    DISPLAY="$DISPLAY_TEST" APP_DATA_ROOT="$1" INBE_NO_TRAY=1 \
        setsid "$BIN" >"$WORK/$2" 2>&1 &
    APP_PID=$!
    sleep 8
}

fresh_root() { # $1 = root; prepares an empty data root
    rm -rf "$1"
    mkdir -p "$1"
}

set_setting() { # $1 = root, $2 = key, $3 = value
    sqlite3 "$1/breathing.db" \
        "insert into settings(user_id,key,value,updated_at) select id,'$2','$3',strftime('%s','now') from users limit 1 on conflict(user_id,key) do update set value=excluded.value, updated_at=excluded.updated_at"
}

# --- environment -----------------------------------------------------------
DISPLAY="$DISPLAY_TEST" setsid xfwm4 >/dev/null 2>&1 &
WM_PID=$!
sleep 1.5

# Fresh profile template: run once with breaks enabled so the DB exists.
ROOT0="$WORK/r0"
fresh_root "$ROOT0"
launch_app "$ROOT0" log0
kill "$APP_PID" 2>/dev/null; APP_PID=""; sleep 1
# The profile's tables may still live in the WAL; checkpoint before copying.
sqlite3 "$ROOT0/breathing.db" "PRAGMA wal_checkpoint(TRUNCATE);" >/dev/null 2>&1
rm -f "$ROOT0/breathing.db-wal" "$ROOT0/breathing.db-shm"
cp "$ROOT0/breathing.db" "$WORK/template.db"
# --- 1. startup maps the window -------------------------------------------
ROOT1="$WORK/r-keep"
fresh_root "$ROOT1"; cp "$WORK/template.db" "$ROOT1/breathing.db"
mkdir -p "$ROOT1/runtime-assets"
set_setting "$ROOT1" desktop_close_action 1   # KEEP_RUNNING
launch_app "$ROOT1" log-keep
MW=$(main_window)
if [ -n "$MW" ] && XW -id "$MW" | grep -q IsViewable; then
    ok "startup: main window maps"
else
    bad "startup: main window does not map"
fi

# --- 2. close = keep running -> hidden but alive ---------------------------
if CB=$(close_button); then
    XD mousemove ${CB% *} ${CB#* }
    XD click 1
    sleep 2.5
    MW=$(main_window)
    if kill -0 "$APP_PID" 2>/dev/null && \
       XW -id "$MW" | grep -q IsUnMapped; then
        ok "close-keep: window hidden, app alive"
    else
        bad "close-keep: expected hidden window with live process"
    fi
else
    bad "close-keep: no WM frame found"
fi

# --- 3. close = ask -> prompt -> Quit -> exits -----------------------------
ROOT2="$WORK/r-ask"
fresh_root "$ROOT2"; cp "$WORK/template.db" "$ROOT2/breathing.db"
mkdir -p "$ROOT2/runtime-assets"
set_setting "$ROOT2" desktop_close_action 0   # ASK
launch_app "$ROOT2" log-ask
if CB=$(close_button); then
    XD mousemove ${CB% *} ${CB#* }
    XD click 1
    sleep 2
    # The prompt is proven behaviorally: the right-hand button only quits
    # when the close prompt is up and accepting clicks.
    XD mousemove 715 482
    XD mousedown 1; sleep 0.15; XD mouseup 1
    sleep 2.5
    if kill -0 "$APP_PID" 2>/dev/null; then
        bad "close-ask: prompt Quit did not exit the app"
    else
        ok "close-ask: prompt appears and Quit exits the app"
        APP_PID=""
    fi
else
    bad "close-ask: no WM frame found"
fi

# --- 4. close = ask -> prompt -> Keep running ------------------------------
ROOT3="$WORK/r-ask2"
fresh_root "$ROOT3"; cp "$WORK/template.db" "$ROOT3/breathing.db"
mkdir -p "$ROOT3/runtime-assets"
set_setting "$ROOT3" desktop_close_action 0   # ASK
launch_app "$ROOT3" log-ask2
if CB=$(close_button); then
    XD mousemove ${CB% *} ${CB#* }
    XD click 1
    sleep 2
    # Left-hand prompt button is Keep running.
    XD mousemove 520 482
    XD mousedown 1; sleep 0.15; XD mouseup 1
    sleep 2.5
    MW=$(main_window)
    if kill -0 "$APP_PID" 2>/dev/null && \
       XW -id "$MW" | grep -q IsUnMapped; then
        ok "close-ask-keep: Keep running hides window, app alive"
    else
        bad "close-ask-keep: expected hidden window with live process"
    fi
else
    bad "close-ask-keep: no WM frame found"
fi

# Break HUD dragging used to live here, but this harness deliberately runs with
# INBE_NO_TRAY=1 so close-window behavior can be tested without a desktop tray.
# HUD behavior requires a separate tray-ready harness.

say ""
say "desktop window-mode tests: $PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ]
