#!/bin/sh
# End-to-end desktop window tests for Inner Breeze.
#
# Drives the real binary on a private Xvfb display with a window manager,
# using xdotool, exactly like a user: clicks the title-bar close button,
# answers the close prompt, and drags the break HUD.
#
# Covered cases (the "closing windows in different inbe modes" matrix):
#   1. startup        main window maps on a fresh profile
#   2. close-keep     close action = keep running -> window hides, app lives
#   3. close-ask      close action = ask -> prompt modal -> Quit -> app exits
#   4. close-ask-keep close action = ask -> prompt -> Keep running -> hidden
#   5. hud-drag       break HUD window moves when dragged
#
# Requirements: Xvfb, xfwm4, xdotool, xwininfo (x11-utils), ImageMagick
# (import/compare), sqlite3. Everything runs on :79 in temp roots; the
# user's session and data are never touched.
#
# Usage: scripts/test-desktop-windows.sh [path-to-binary]

set -u

BIN=${1:-./build/bin/linux/inbe-linux-x86_64}
DISPLAY_TEST=":79"
GEOMETRY="1280x800x24"
WORK=$(mktemp -d /tmp/inbe-winmode.XXXXXX)
PASS=0
FAIL=0
APP_PID=""

[ -x "$BIN" ] || { echo "FAIL: binary not found: $BIN"; exit 1; }
for tool in Xvfb xfwm4 xdotool xwininfo import compare sqlite3; do
    command -v "$tool" >/dev/null 2>&1 || {
        echo "FAIL: missing tool: $tool (install xvfb, xfwm4, xdotool, x11-utils, imagemagick, sqlite3)"
        exit 1
    }
done

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

# Break HUD window id: named "Inner Breeze", 193px wide.
hud_window() {
    for w in $(XD search --name "Inner Breeze"); do
        width=$(XD getwindowgeometry --shell "$w" | grep '^WIDTH' | cut -d= -f2)
        [ "${width:-0}" = "193" ] && { echo "$w"; return; }
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
    DISPLAY="$DISPLAY_TEST" INBE_DATA_ROOT="$1" INBE_NO_TRAY=1 \
        setsid "$BIN" >"$WORK/$2" 2>&1 &
    APP_PID=$!
    sleep 8
}

fresh_root() { # $1 = root; prepares an empty data root
    rm -rf "$1"
    mkdir -p "$1"
}

set_setting() { # $1 = root, $2 = key, $3 = value
    sqlite3 "$1/inbe.db" "update settings set value='$3' where key='$2'"
}

# --- environment -----------------------------------------------------------
Xvfb "$DISPLAY_TEST" -screen 0 "$GEOMETRY" >/dev/null 2>&1 &
XVFB_PID=$!
sleep 1.5
DISPLAY="$DISPLAY_TEST" setsid xfwm4 >/dev/null 2>&1 &
WM_PID=$!
sleep 1.5

# Fresh profile template: run once with breaks enabled so the DB exists.
ROOT0="$WORK/r0"
fresh_root "$ROOT0"
launch_app "$ROOT0" log0
kill "$APP_PID" 2>/dev/null; APP_PID=""; sleep 1
# The profile's tables may still live in the WAL; checkpoint before copying.
sqlite3 "$ROOT0/inbe.db" "PRAGMA wal_checkpoint(TRUNCATE);" >/dev/null 2>&1
rm -f "$ROOT0/inbe.db-wal" "$ROOT0/inbe.db-shm"
cp "$ROOT0/inbe.db" "$WORK/template.db"
# Template profile defaults used by every case below:
#   breaks enabled (so the HUD window exists), close action set per case.
sqlite3 "$WORK/template.db" \
    "update settings set value='1' where key='breaks_enabled'"

# --- 1. startup maps the window -------------------------------------------
ROOT1="$WORK/r-keep"
fresh_root "$ROOT1"; cp "$WORK/template.db" "$ROOT1/inbe.db"
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
fresh_root "$ROOT2"; cp "$WORK/template.db" "$ROOT2/inbe.db"
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
fresh_root "$ROOT3"; cp "$WORK/template.db" "$ROOT3/inbe.db"
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

# --- 5. HUD drag ------------------------------------------------------------
launch_app "$ROOT3" log-drag
HUD=$(hud_window)
if [ -n "$HUD" ]; then
    eval $(XD getwindowgeometry --shell "$HUD")
    bx=$X; by=$Y
    # Drag along a precise path; the window must follow the pointer exactly.
    XD mousemove $((X + 90)) $((Y + 20))
    XD mousedown 1
    for d in 20 40 60; do
        XD mousemove $((X + 90 - d)) $((Y + 20 + d / 3))
        sleep 0.2
    done
    XD mouseup 1
    sleep 1.5
    eval $(XD getwindowgeometry --shell "$HUD")
    ex=$((bx - 60)); ey=$((by + 20))
    dx=$((${X#-} - ${ex#-})); [ ${dx#-} -lt 0 ] && dx=$((-dx))
    dy=$((${Y#-} - ${ey#-})); [ ${dy#-} -lt 0 ] && dy=$((-dy))
    if [ "$dx" -le 2 ] && [ "$dy" -le 2 ]; then
        ok "hud-drag: window follows pointer exactly ($bx,$by -> $X,$Y)"
    elif [ $((X + Y)) -ne $((bx + by)) ]; then
        bad "hud-drag: moved but off target ($bx,$by -> $X,$Y, want $ex,$ey)"
    else
        bad "hud-drag: window did not move"
    fi
else
    bad "hud-drag: break HUD window not found"
fi

say ""
say "desktop window-mode tests: $PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ]
