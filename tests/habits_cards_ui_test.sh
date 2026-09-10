#!/usr/bin/env bash
set -euo pipefail
# Run under xvfb-run. Screenshot mode seeds its own temporary database.
binary="${1:?native binary required}"
test_dir="$(mktemp -d /tmp/breathing-habits-ui.XXXXXX)"
APP_SHOT_WINDOW=1 "$binary" --screenshot "$test_dir/start.png" \
  --screenshot-scene habits_overview --screenshot-width 411 --screenshot-height 813 \
  --screenshot-theme 4 --screenshot-dark 1 --screenshot-style 2 >"$test_dir/app.log" 2>&1 &
app_pid=$!
trap 'kill "$app_pid" 2>/dev/null || true' EXIT
db="/tmp/breathing-screenshot-$app_pid/breathing.db"
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
# The current focus layout starts with Meditation expanded and Yoga collapsed.
tap 160 520
expect "$(sql "SELECT name FROM habits WHERE id=(SELECT value FROM settings WHERE key='habits_selected_id')")" 'Yoga' 'collapsed card opens correct habit'
import -window "$window" "$test_dir/yoga-expanded.png"
# Yoga is now the second card: the first collapsed card occupies 152 units.
tap 50 508
expect "$(day_count)" "$((before+1))" 'today check-in'
expect "$(sql "SELECT local_date FROM habit_days WHERE habit_id=(SELECT id FROM habits WHERE name='Yoga') AND completed=1")" "$(date +%Y%m%d)" 'leftmost circle marks today'
tap 50 508
expect "$(day_count)" "$before" 'today undo'
tap 99 508
expect "$(day_count)" "$((before+1))" 'past-day check-in'
tap 99 508
expect "$(day_count)" "$before" 'past-day undo'
drag 50 508 294 508
expect "$(day_count)" "$before" 'drag does not check a day'
tap 160 260
expect "$(sql "SELECT value FROM settings WHERE key='habits_selected_id'")" '' 'selected card header collapses'
import -window "$window" "$test_dir/collapsed.png"
# Card title dragging is still owned by the reorder controller.
drag 160 110 160 390
expect "$(order)" 'Yoga|Meditation|Sit ups|Push ups|Cold Shower|Jumping Rope' 'card reorder'
expect "$(day_count)" "$before" 'reorder preserves check-ins'
import -window "$window" "$test_dir/reordered.png"
echo "PASS shared habit buttons: selection, day taps, undo, drag cancellation, collapse and reorder"
echo "Screenshots and isolated database: $test_dir ; $db"
