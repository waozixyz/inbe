#!/bin/sh
set -eu

sdk_root=${ANDROID_SDK_ROOT:-${ANDROID_HOME:-${1:-}}}
app_id=${2:-xyz.waozi.inbe}
activity=${3:-xyz.waozi.inbe.MainActivity}

if [ -z "$sdk_root" ]; then
    echo "android smoke: set ANDROID_SDK_ROOT or ANDROID_HOME"
    exit 2
fi

emulator="$sdk_root/emulator/emulator"
if [ ! -x "$emulator" ]; then
    echo "android smoke: Android Emulator package is missing: $emulator"
    echo "android smoke: install it with: sdkmanager \"emulator\""
    exit 2
fi

bash scripts/emulator.sh

adb_cmd="$sdk_root/platform-tools/adb"
if [ ! -x "$adb_cmd" ]; then
    adb_cmd=adb
fi

"$adb_cmd" -e wait-for-device
"$adb_cmd" -e logcat -c || true
${MAKE:-make} android-install ADB="$adb_cmd -e"
sleep 5
"$adb_cmd" -e shell am start -n "$app_id/$activity" >/dev/null
sleep 3
"$adb_cmd" -e logcat -d -t 400 > build/android/android-smoke-logcat.txt

if grep -E "FATAL EXCEPTION|Fatal signal|AndroidRuntime|DEBUG.*backtrace" build/android/android-smoke-logcat.txt >/dev/null; then
    echo "android smoke: FAIL; see build/android/android-smoke-logcat.txt"
    exit 1
fi

echo "android smoke: PASS"
