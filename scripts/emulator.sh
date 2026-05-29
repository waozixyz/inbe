#!/usr/bin/env bash

set -e

AVD_NAME="inbe-test"
APK_PATH="build/android/app-universal-debug.apk"
PACKAGE_NAME="xyz.waozi.inbe"

echo "🚀 Complete Android Emulator Setup"
echo "   This script will:"
echo "   1. Create AVD if missing"
echo "   2. Launch emulator"
echo "   3. Build APK"
echo "   4. Install and run app"
echo ""

# Check for ANDROID_HOME
if [ -z "$ANDROID_HOME" ]; then
  echo "❌ Error: ANDROID_HOME environment variable not set"
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

# Step 2: Build APK
echo "🔨 Building APK..."
make android-debug
echo "✅ APK built: $APK_PATH"
echo ""

# Step 3: Check if emulator is running
if adb devices | grep -q "emulator"; then
  echo "✅ Emulator already running"
else
  echo "🚀 Launching emulator..."

  # Set environment for emulator
  export ANDROID_HOME="$ANDROID_SDK_ROOT"
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

# Step 4: Install and run app
echo "📦 Installing APK..."
adb install -r "$APK_PATH"

echo "🚀 Launching app..."
adb shell am start -n "$PACKAGE_NAME/.MainActivity"

echo ""
echo "🎉 App is running on emulator!"
echo ""
echo "To stop the emulator:"
echo "  adb emu kill"
echo ""
echo "To rebuild and restart:"
echo "  ./scripts/emulator.sh"
