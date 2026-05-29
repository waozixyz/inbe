#!/usr/bin/env bash

set -e

AVD_NAME="inbe-test"

echo "🚀 Android Emulator Manager"
echo "   This script handles emulator creation and launching"
echo "   Use './scripts/run-android.sh' to build and install the app"
echo ""

# Check for Android SDK location
if [ -z "$ANDROID_SDK_ROOT" ] && [ -z "$ANDROID_HOME" ]; then
  echo "❌ Error: Neither ANDROID_SDK_ROOT nor ANDROID_HOME environment variable is set"
  echo "   Make sure to run this script within 'nix develop'"
  exit 1
fi

# Set up writable SDK location
WRITABLE_SDK="/mnt/storage/Android/Sdk"
if [ -d "$WRITABLE_SDK" ] && [ -w "$WRITABLE_SDK" ]; then
  export ANDROID_SDK_ROOT="$WRITABLE_SDK"
  export ANDROID_SDK_HOME="$WRITABLE_SDK"
  export ANDROID_AVD_HOME="$WRITABLE_SDK/avd"
  echo "📂 Using writable Android SDK: $ANDROID_SDK_ROOT"
else
  echo "❌ Error: No writable Android SDK location found"
  exit 1
fi

# Step 1: Create AVD if missing
if [ ! -d "$ANDROID_SDK_ROOT/avd/$AVD_NAME.avd" ]; then
  echo "📱 AVD not found. Creating..."
  bash scripts/create-avd.sh "$AVD_NAME"
  echo ""
else
  echo "✅ AVD already exists: $AVD_NAME"
  echo ""
fi

# Step 2: Check if emulator is running
if adb devices | grep -q "emulator"; then
  echo "✅ Emulator already running"
  echo ""
  echo "To stop the emulator:"
  echo "  adb emu kill"
else
  echo "🚀 Launching emulator..."

  # Set environment for emulator (unset ANDROID_HOME to avoid conflicts)
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

  echo "⏳ Waiting for emulator to boot..."
  timeout_seconds=120
  elapsed=0

  while ! adb shell getprop sys.boot_completed 2>/dev/null | grep -q "1"; do
    if [ $elapsed -ge $timeout_seconds ]; then
      echo "❌ Timeout waiting for emulator to boot"
      echo "   Check /tmp/emulator.log for details"
      exit 1
    fi

    echo -n "⏳ "
    date +"%T"
    sleep 5
    elapsed=$((elapsed + 5))

    # Check if emulator process died
    if ! pgrep -f "emulator.*$AVD_NAME" > /dev/null; then
      echo "❌ Emulator process died"
      echo "   Check /tmp/emulator.log for details"
      exit 1
    fi
  done

  echo "✅ Emulator booted successfully"
  echo ""
fi

echo "📱 Emulator ready!"
echo ""
echo "Next steps:"
echo "  1. Build and install the app:"
echo "     ./scripts/run-android.sh"
