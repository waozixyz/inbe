#!/usr/bin/env bash

set -e

AVD_NAME="${AVD_NAME:-inbe-test}"

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
  echo "ℹ️  AVD not found. Creating new Pixel 8 Pro AVD..."
  bash scripts/create-avd.sh "$AVD_NAME"

  # Re-export after creation
  export ANDROID_SDK_ROOT="$PERSISTENT_SDK_ROOT"
  export ANDROID_SDK_HOME="$PERSISTENT_SDK_ROOT"
  export ANDROID_AVD_HOME="$PERSISTENT_SDK_ROOT/avd"
fi

ADB_CMD="${ANDROID_SDK_ROOT}/platform-tools/adb"
if [ ! -x "$ADB_CMD" ]; then
  ADB_CMD="$(command -v adb)"
fi

EMULATOR_CMD="${ANDROID_SDK_ROOT}/emulator/emulator"
if [ ! -x "$EMULATOR_CMD" ]; then
  EMULATOR_CMD="$(command -v emulator)"
fi

# Check if emulator is already running
if "$ADB_CMD" devices | grep -q '^emulator-[0-9][0-9]*[[:space:]]*device'; then
  echo "✅ Emulator already running"
  "$ADB_CMD" devices
  exit 0
fi

echo "🚀 Launching Pixel 8 Pro emulator (Android 14 API 34)..."

# Set environment for emulator
unset ANDROID_HOME
export ANDROID_SDK_ROOT="$ANDROID_SDK_ROOT"

# Launch emulator in background with KVM acceleration
"$EMULATOR_CMD" @"$AVD_NAME" \
  -gpu host \
  -skin 1440x2960 \
  -no-snapshot-load \
  -no-boot-anim \
  -verbose \
  -qemu -enable-kvm > /tmp/emulator.log 2>&1 &

echo "⏳ Waiting for boot (this may take a while)..."
timeout_seconds=120
elapsed=0

timeout 20 "$ADB_CMD" -e wait-for-device >/dev/null 2>&1 || true
while ! "$ADB_CMD" -e shell getprop sys.boot_completed 2>/dev/null | grep -q "1"; do
  if [ $elapsed -ge $timeout_seconds ]; then
    echo "❌ Timeout waiting for boot. Check /tmp/emulator.log"
    exit 1
  fi

  sleep 5
  elapsed=$((elapsed + 5))

  if ! pgrep -f "emulator.*$AVD_NAME" > /dev/null; then
    echo "❌ Emulator died. Check /tmp/emulator.log"
    tail -20 /tmp/emulator.log
    exit 1
  fi

  # Show progress
  if [ $((elapsed % 15)) -eq 0 ]; then
    echo "   Still booting... (${elapsed}s)"
  fi
done

echo "✅ Pixel 8 Pro emulator ready!"
echo ""
echo "📱 Device info:"
"$ADB_CMD" -e shell getprop ro.product.model
"$ADB_CMD" -e shell getprop ro.build.version.release
"$ADB_CMD" -e shell getprop ro.build.version.sdk
echo ""
echo "   Connect with: $ADB_CMD -e shell"
