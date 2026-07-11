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
OUT_DIR="${SCREENSHOT_OUT_DIR:-"$ROOT_DIR/tmp"}"
DATA_BASE="$OUT_DIR/screenshot-data"

SCENES=(
  "home:01-home:0:0"
  "background_music:02-background-music:1:0"
  "practice_config_whm:03-wim-hof-config:2:0"
  "practice_manual_whm:04-wim-hof-manual:3:0"
  "wim_hof_session:05-wim-hof-session:11:1"
  "calendar_meditation:06-calendar-meditation:1:0"
  "habits_stats:07-habits-statistics:2:1"
  "data:08-data:0:0"
  "my_practices:09-my-practices:4:0"
  "configure_account:10-configure-account:7:1"
  "settings_session:11-settings-session:0:0"
  "theme_selection:12-theme-selection:0:0"
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
  echo "Install xvfb-run and ensure it is on PATH." >&2
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

  if [[ ! -f "$data_root/inbe.db" ]]; then
    NOW=$(date +%s)
    USER_ID="local-screenshot-$NOW"

    # Calculate today's date in YYYYMMDD format
    TODAY_DATE=$(date +%Y%m%d)

    sqlite3 "$data_root/inbe.db" <<EOF
CREATE TABLE users(
 id TEXT PRIMARY KEY,
 created_at INTEGER NOT NULL,
 kind TEXT NOT NULL
);
CREATE TABLE settings(
 user_id TEXT NOT NULL,
 key TEXT NOT NULL,
 value TEXT NOT NULL,
 updated_at INTEGER NOT NULL,
 PRIMARY KEY(user_id,key)
);
CREATE TABLE meta(
 key TEXT PRIMARY KEY,
 value TEXT
);
CREATE TABLE habits(
 id TEXT PRIMARY KEY,
 user_id TEXT NOT NULL,
 name TEXT NOT NULL,
 color_r INTEGER NOT NULL,
 color_g INTEGER NOT NULL,
 color_b INTEGER NOT NULL,
 sync_mode INTEGER NOT NULL,
 sync_activity INTEGER NOT NULL,
 counter_enabled INTEGER NOT NULL DEFAULT 0,
 sort_order INTEGER NOT NULL,
 deleted_at INTEGER NOT NULL DEFAULT 0
);
CREATE TABLE habit_days(
 habit_id TEXT NOT NULL,
 local_date INTEGER NOT NULL,
 completed INTEGER NOT NULL,
 count INTEGER NOT NULL DEFAULT 0,
 session_count INTEGER NOT NULL DEFAULT 0,
 updated_at INTEGER NOT NULL,
 PRIMARY KEY(habit_id,local_date)
);
CREATE TABLE sessions(
 id TEXT PRIMARY KEY,
 user_id TEXT NOT NULL,
 started_at INTEGER NOT NULL,
 local_date INTEGER NOT NULL,
 topic INTEGER NOT NULL DEFAULT 0,
 activity INTEGER NOT NULL DEFAULT 0,
 source TEXT NOT NULL,
 imported_at INTEGER NOT NULL,
 rounds_hash INTEGER NOT NULL,
 deleted_at INTEGER NOT NULL DEFAULT 0,
 updated_at INTEGER NOT NULL DEFAULT 0,
 UNIQUE(user_id,started_at,rounds_hash)
);
CREATE TABLE session_rounds(
 session_id TEXT NOT NULL,
 round_index INTEGER NOT NULL,
 seconds INTEGER NOT NULL,
 PRIMARY KEY(session_id,round_index)
);
CREATE TABLE imports(
 id TEXT PRIMARY KEY,
 imported_at INTEGER NOT NULL,
 format TEXT NOT NULL,
 source_name TEXT NOT NULL,
 session_count INTEGER NOT NULL,
 habit_count INTEGER NOT NULL
);
CREATE TABLE sync_outbox(
 seq INTEGER PRIMARY KEY AUTOINCREMENT,
 entity_type TEXT NOT NULL,
 entity_id TEXT NOT NULL,
 local_date INTEGER NOT NULL DEFAULT 0,
 queued_at INTEGER NOT NULL,
 UNIQUE(entity_type,entity_id,local_date)
);
INSERT INTO users(id, created_at, kind) VALUES ('$USER_ID', $NOW, 'local');
INSERT INTO settings(user_id,key,value,updated_at) VALUES ('$USER_ID', 'tutorial_seen', '1', $NOW);
INSERT INTO settings(user_id,key,value,updated_at) VALUES ('$USER_ID', 'habits_guide_seen', '1', $NOW);
INSERT INTO settings(user_id,key,value,updated_at) VALUES ('$USER_ID', 'schema_version', '11', $NOW);

-- Habits (sync_activity: 1=WIM_HOF, 2=MEDITATION, 3=both)
INSERT INTO habits(id, user_id, name, color_r, color_g, color_b, sync_mode, sync_activity, counter_enabled, sort_order, deleted_at) VALUES
('habit-1', '$USER_ID', 'Meditation', 126, 183, 230, 1, 3, 0, 0, 0),
('habit-2', '$USER_ID', 'Exercise', 150, 200, 150, 1, 1, 1, 1, 0),
('habit-3', '$USER_ID', 'Yoga', 200, 150, 200, 1, 2, 0, 2, 0),
('habit-4', '$USER_ID', 'Mindfulness', 200, 180, 130, 1, 3, 1, 3, 0),
('habit-5', '$USER_ID', 'Breathing', 150, 180, 220, 1, 1, 0, 4, 0),
('habit-6', '$USER_ID', 'Stretching', 220, 180, 150, 1, 2, 1, 5, 0);
EOF

    # Generate habit completion data for each habit for the last 28 days
    for habit_num in {1..6}; do
      habit_id="habit-$habit_num"
      for day_offset in {0..27}; do
        local_date=$(date -d "$day_offset days ago" +%Y%m%d)
        completed=$((RANDOM % 2))  # Random 0 or 1
        habit_day_count=$((completed > 0 ? 1 + (RANDOM % 3) : 0))  # 1-3 if completed, 0 otherwise
        updated_at=$(date -d "$day_offset days ago" +%s)

        sqlite3 "$data_root/inbe.db" "INSERT INTO habit_days(habit_id, local_date, completed, count, session_count, updated_at) VALUES ('$habit_id', $local_date, $completed, $habit_day_count, 0, $updated_at);"
      done
    done

    # Generate varied session data for the last 28 days
    session_count=0
    for day_offset in {0..27}; do
      local_date=$(date -d "$day_offset days ago" +%Y%m%d)
      day_start_time=$(date -d "$day_offset days ago" +%s)

      # Decide session pattern: 40% no sessions, 40% 1 session, 20% 2 sessions
      session_pattern=$((RANDOM % 10))
      num_sessions=0
      if [ $session_pattern -lt 4 ]; then
        num_sessions=0
      elif [ $session_pattern -lt 8 ]; then
        num_sessions=1
      else
        num_sessions=2
      fi

      for session_num in $(seq 1 $num_sessions); do
        # Only WHM sessions (topic=0, activity=0)
        topic=0

        # Vary start times throughout the day
        hour=$((8 + RANDOM % 12))
        minute=$((RANDOM % 60))
        started_at=$((day_start_time + hour * 3600 + minute * 60))
        updated_at=$((started_at + 300))  # Session ends ~5 min later

        session_id="session-$session_count"
        session_count=$((session_count + 1))

        # Generate round hash (just use timestamp)
        rounds_hash=$started_at

        sqlite3 "$data_root/inbe.db" "INSERT INTO sessions(id, user_id, started_at, local_date, topic, activity, source, imported_at, rounds_hash, deleted_at, updated_at) VALUES ('$session_id', '$USER_ID', $started_at, $local_date, $topic, 0, 'screenshot', 0, $rounds_hash, 0, $updated_at);"

        # Generate rounds with varied durations
        if [ $topic -eq 0 ]; then
          # WHM: 3-5 rounds, 30-90 seconds each
          num_rounds=$((3 + RANDOM % 3))
          for round_index in $(seq 0 $((num_rounds - 1))); do
            seconds=$((30 + RANDOM % 61))  # 30-90 seconds
            sqlite3 "$data_root/inbe.db" "INSERT INTO session_rounds(session_id, round_index, seconds) VALUES ('$session_id', $round_index, $seconds);"
          done
        fi
      done
    done
  fi

  set +e
  INBE_DATA_ROOT="$data_root" xvfb-run -a -s "-screen 0 ${width}x${height}x24" \
    "$BIN" \
    --screenshot "$output" \
    --screenshot-scene "$scene" \
    --screenshot-width "$width" \
    --screenshot-height "$height" \
    --screenshot-theme "$theme" \
    --screenshot-dark "$dark"
  app_status=$?
  set -e
  if [[ "$app_status" -ne 0 && ! -s "$output" ]]; then
    echo "Screenshot app failed for scene '$scene' with status $app_status" >&2
    exit "$app_status"
  fi
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
