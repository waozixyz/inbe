#!/usr/bin/env bash

set -e

AVD_NAME="inbe-test"

# Check for Android SDK location
if [ -z "$ANDROID_SDK_ROOT" ] && [ -z "$ANDROID_HOME" ]; then
  echo "❌ Error: Neither ANDROID_SDK_ROOT nor ANDROID_HOME is set"
  exit 1
fi

# Set up writable SDK location
PERSISTENT_SDK_ROOT="$HOME/.android-sdk-writable"

if [ -d "$PERSISTENT_SDK_ROOT" ] && [ -d "$PERSISTENT_SDK_ROOT/avd/$AVD_NAME.avd" ]; then
  export ANDROID_SDK_ROOT="$PERSISTENT_SDK_ROOT"
  export ANDROID_SDK_HOME="$PERSISTENT_SDK_ROOT"
  export ANDROID_AVD_HOME="$PERSISTENT_SDK_ROOT/avd"
else
  echo "❌ Error: AVD not found. Run: bash scripts/create-avd.sh"
  exit 1
fi

# Create AVD if missing
if [ ! -d "$ANDROID_SDK_ROOT/avd/$AVD_NAME.avd" ]; then
  bash scripts/create-avd.sh "$AVD_NAME"
fi

# Check if emulator is running
if adb devices | grep -q "emulator"; then
  echo "✅ Emulator already running"
  exit 0
fi

echo "🚀 Launching emulator..."

# Set environment for emulator
unset ANDROID_HOME
export ANDROID_SDK_ROOT="$ANDROID_SDK_ROOT"

# Launch emulator in background
emulator @"$AVD_NAME" \
  -gpu host \
  -skin 1080x2400 \
  -no-snapshot-load \
  -no-boot-anim \
  -show-kernel \
  -verbose > /tmp/emulator.log 2>&1 &

echo "⏳ Waiting for boot..."
timeout_seconds=120
elapsed=0

while ! adb shell getprop sys.boot_completed 2>/dev/null | grep -q "1"; do
  if [ $elapsed -ge $timeout_seconds ]; then
    echo "❌ Timeout. Check /tmp/emulator.log"
    exit 1
  fi

  sleep 5
  elapsed=$((elapsed + 5))

  if ! pgrep -f "emulator.*$AVD_NAME" > /dev/null; then
    echo "❌ Emulator died. Check /tmp/emulator.log"
    exit 1
  fi
done

echo "✅ Ready!"
