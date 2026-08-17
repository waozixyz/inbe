#!/usr/bin/env bash
set -euo pipefail

app_id=xyz.waozi.inbe
activity=xyz.waozi.inbe.MainActivity
avd_name=inbe-audio-test-api30
emulator_port=${ANDROID_AUDIO_E2E_PORT:-5580}
serial="emulator-$emulator_port"
work_dir=build/android-audio-e2e
fixture=test-fixtures/audio/autumn-sunset.mp3
fixture_sha256=d1f25feccd8af5cfe22ca1ed6afa901a5f97cbf976b93fc46096a2bd83315034
capture_device=
recorder_pid=
previous_sink=

for command in adb ffmpeg ffprobe pactl parec python3 sha256sum; do
    command -v "$command" >/dev/null || { echo "android audio e2e: missing $command"; exit 2; }
done

cleanup() {
    if [ -n "$recorder_pid" ]; then kill "$recorder_pid" 2>/dev/null || true; fi
    adb -s "$serial" emu kill >/dev/null 2>&1 || true
}
trap cleanup EXIT INT TERM

mkdir -p "$work_dir"
printf '%s  %s\n' "$fixture_sha256" "$fixture" | sha256sum -c -
ffprobe -v error -select_streams a:0 \
    -show_entries stream=codec_name,channels,sample_rate,bit_rate \
    -of default=nw=1 "$fixture" | tee "$work_dir/fixture.txt"
grep -q '^codec_name=mp3$' "$work_dir/fixture.txt"
grep -q '^channels=2$' "$work_dir/fixture.txt"
grep -q '^bit_rate=128000$' "$work_dir/fixture.txt"

previous_sink=$(pactl get-default-sink)
capture_device="$previous_sink.monitor"
if ! pactl list short sources | grep -q "[[:space:]]$capture_device[[:space:]]"; then
    echo "android audio e2e: default output has no monitor source: $previous_sink"
    exit 2
fi
unset PULSE_SINK QEMU_PA_SINK
export QEMU_AUDIO_DRV=pa
export AVD_NAME="$avd_name"
export ANDROID_EMULATOR_PORT="$emulator_port"
export ANDROID_EMULATOR_AUDIO_BACKEND=pulse
export ANDROID_API=30
export ANDROID_AVD_RAM_MB=2048
export ANDROID_AVD_HEAP_MB=512
export ANDROID_EMULATOR_RAM_MB=2048
export ANDROID_SYSTEM_IMAGE_TYPE=google_apis
bash scripts/emulator.sh

adb -s "$serial" wait-for-device
adb -s "$serial" logcat -c
make android-debug
apk=droid/app/build/outputs/apk/debug/app-x86_64-debug.apk
if [ ! -f "$apk" ]; then apk=droid/app/build/outputs/apk/debug/app-debug.apk; fi
adb -s "$serial" install -r "$apk"
adb -s "$serial" shell pm clear "$app_id" >/dev/null
adb -s "$serial" shell am start -n "$app_id/$activity" >/dev/null
sleep 5
adb -s "$serial" shell cmd media_session volume --stream 3 --set 10 >/dev/null

adb -s "$serial" push "$fixture" /data/local/tmp/inbe-audio-e2e.mp3 >/dev/null
adb -s "$serial" shell run-as "$app_id" cp /data/local/tmp/inbe-audio-e2e.mp3 files/inbe-audio-e2e.mp3
adb -s "$serial" shell am start -n "$app_id/$activity" \
    -a xyz.waozi.inbe.action.AUDIO_E2E_IMPORT \
    --es audio_file_name inbe-audio-e2e.mp3 --ei practice_id 0 >/dev/null

for _ in $(seq 1 30); do
    if adb -s "$serial" logcat -d | grep -q 'AUDIO_E2E: import ready'; then break; fi
    sleep 1
done
adb -s "$serial" logcat -d > "$work_dir/logcat-import.txt"
grep -q 'AUDIO_E2E: import ready' "$work_dir/logcat-import.txt"
sleep 2

ffmpeg -y -v error -i "$fixture" -t 10 -ac 1 -ar 8000 -c:a pcm_s16le "$work_dir/source.wav"
capture_practice_audio() {
    label=$1
    parec --device="$capture_device" --file-format=wav "$work_dir/capture-$label-raw.wav" &
    recorder_pid=$!
    adb -s "$serial" shell am start -n "$app_id/$activity" \
        -a xyz.waozi.inbe.action.START_PRACTICE --ei practice_id 0 >/dev/null
    sleep 12
    kill "$recorder_pid" 2>/dev/null || true
    wait "$recorder_pid" 2>/dev/null || true
    recorder_pid=
    ffmpeg -y -v error -i "$work_dir/capture-$label-raw.wav" \
        -ac 1 -ar 8000 -c:a pcm_s16le "$work_dir/capture-$label.wav"
    adb -s "$serial" logcat -d > "$work_dir/logcat-$label.txt"
    pactl list short sink-inputs > "$work_dir/sink-inputs-$label.txt"
    python3 scripts/audio-envelope-match.py "$work_dir/source.wav" \
        "$work_dir/capture-$label.wav" | tee "$work_dir/audio-match-$label.txt"
}

capture_practice_audio initial

adb -s "$serial" logcat -d > "$work_dir/logcat-playback.txt"
if grep -E 'Could not start track|session music is not playing|FATAL EXCEPTION|Fatal signal' "$work_dir/logcat-playback.txt"; then
    echo "android audio e2e: playback error"
    exit 1
fi

adb -s "$serial" shell am force-stop "$app_id"
adb -s "$serial" shell am start -n "$app_id/$activity" >/dev/null
sleep 4
capture_practice_audio relaunch
adb -s "$serial" logcat -d > "$work_dir/logcat-relaunch.txt"

echo "android audio e2e: PASS; artifacts in $work_dir"
