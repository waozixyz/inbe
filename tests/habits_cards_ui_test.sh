#!/usr/bin/env bash
set -euo pipefail
# Run under xvfb-run. Screenshot mode seeds its own temporary database.
binary="${1:?native binary required}"
test_dir="$(mktemp -d /tmp/inbe-habits-ui.XXXXXX)"
INBE_SHOT_WINDOW=1 "$binary" --screenshot "$test_dir/start.png" \
  --screenshot-scene habits_overview --screenshot-width 411 --screenshot-height 813 \
  --screenshot-theme 4 --screenshot-dark 1 --screenshot-style 2 >"$test_dir/app.log" 2>&1 &
app_pid=$!
trap 'kill "$app_pid" 2>/dev/null || true' EXIT
db="/tmp/inbe-screenshot-$app_pid/inbe.db"
window=""
for attempt in $(seq 1 80); do
  window="$(xdotool search --onlyvisible --pid "$app_pid" 2>/dev/null | head -1 || true)"
  if [[ -n "$window" && -f "$db" ]]; then break; fi
  sleep 0.1
done
[[ -n "$window" && -f "$db" ]] || { echo "FAIL isolated app startup: $test_dir"; exit 1; }
sleep 1
sql() { sqlite3 -cmd '.timeout 2000' "$db" "$1"; }
expect() { [[ "$1" == "$2" ]] || { echo "FAIL $3: got '$1', expected '$2' ($test_dir)"; exit 1; }; }
tap() { xdotool mousemove --window "$window" "$1" "$2" mousedown 1 sleep 0.08 mouseup 1; sleep 0.3; }
drag() {
  xdotool mousemove --window "$window" "$1" "$2" mousedown 1 sleep 0.1
  local start_y="$2" end_y="$4"
  for step in $(seq 1 12); do
    xdotool mousemove --window "$window" "$3" "$((start_y + (end_y-start_y)*step/12))"
    sleep 0.025
  done
  xdotool mouseup 1
  sleep 0.4
}
order() { sql "SELECT group_concat(name,'|') FROM (SELECT name FROM habits WHERE deleted_at=0 ORDER BY sort_order)"; }
day_count() { sql "SELECT COALESCE(SUM(completed),0) FROM habit_days WHERE habit_id=(SELECT id FROM habits WHERE name='Yoga')"; }
baseline="$(order)"
expect "$baseline" 'Meditation|Yoga|Sit ups|Push ups|Cold Shower|Jumping Rope' 'initial order'
import -window "$window" "$test_dir/overview.png"
before="$(day_count)"
tap 65 438
expect "$(day_count)" "$((before+1))" 'today check-in'
expect "$(sql "SELECT local_date FROM habit_days WHERE habit_id=(SELECT id FROM habits WHERE name='Yoga') AND completed=1")" "$(date +%Y%m%d)" 'leftmost circle marks today'
tap 65 438
expect "$(day_count)" "$before" 'today undo'
tap 149 438
expect "$(day_count)" "$((before+1))" 'past-day check-in'
expect "$(sql "SELECT local_date FROM habit_days WHERE habit_id=(SELECT id FROM habits WHERE name='Yoga') AND completed=1")" "$(date -d yesterday +%Y%m%d)" 'second circle marks yesterday'
tap 149 438
expect "$(day_count)" "$before" 'past-day undo'
# A horizontal drag that ends on another circle must not check that day.
drag 65 438 317 438
expect "$(day_count)" "$before" 'drag does not check a day'
# Title drag reorders and preserves day state, even though the page can scroll.
drag 160 173 160 540
expect "$(order)" 'Yoga|Meditation|Sit ups|Push ups|Cold Shower|Jumping Rope' 'card reorder'
expect "$(day_count)" "$before" 'reorder does not mark days'
import -window "$window" "$test_dir/reordered.png"
# Scrolling from a day row must not reorder or mark anything.
drag 200 438 200 260
expect "$(order)" 'Yoga|Meditation|Sit ups|Push ups|Cold Shower|Jumping Rope' 'scroll preserves order'
expect "$(day_count)" "$before" 'scroll does not mark days'
xdotool mousemove --window "$window" 200 400 click --repeat 8 --delay 30 4
sleep 0.3
# Tiny title movements remain taps and open the former ellipsis destination.
tap 160 173
expect "$(sql "SELECT value FROM settings WHERE key='habits_screen_mode'")" '1' 'card opens details'
expect "$(sql "SELECT name FROM habits WHERE id=(SELECT value FROM settings WHERE key='habits_selected_id')")" 'Yoga' 'correct card details'
import -window "$window" "$test_dir/details.png"
tap 130 186
sleep 0.3
expect "$(sql "SELECT value FROM settings WHERE key='habits_screen_mode'")" '0' 'selected card header collapses focus card'
xdotool mousemove --window "$window" 160 173 mousedown 1 sleep 0.1 \
  mousemove --window "$window" 160 690 sleep 1 mouseup 1
sleep 0.4
expect "$(order)" 'Meditation|Sit ups|Push ups|Cold Shower|Jumping Rope|Yoga' 'edge auto-scroll reorder'
expect "$(day_count)" "$before" 'edge drag does not mark days'
import -window "$window" "$test_dir/edge-reorder.png"
echo "PASS habit day taps, undo, drag cancellation, persisted reorder and card navigation"
echo "Screenshots and isolated database: $test_dir ; $db"
