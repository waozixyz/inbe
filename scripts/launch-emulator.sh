#!/usr/bin/env bash

set -e

AVD_NAME="${1:-inbe-test}"

echo "🚀 Launching Android Emulator"
echo "   AVD Name: $AVD_NAME"
echo ""

# Check for ANDROID_HOME
if [ -z "$ANDROID_HOME" ]; then
  echo "❌ Error: ANDROID_HOME environment variable not set"
  echo "   Make sure to run this script within 'nix develop'"
  exit 1
fi

# Set up writable SDK location for AVDs
# Nix store is read-only, so we use the system SDK for AVD storage
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

# Check for required tools
if ! command -v emulator &> /dev/null; then
  echo "❌ Error: emulator command not found in PATH"
  echo "   Make sure emulator is included in your nix environment"
  exit 1
fi

# Check if AVD exists
if [ ! -d "$ANDROID_SDK_ROOT/avd/$AVD_NAME.avd" ]; then
  echo "❌ AVD '$AVD_NAME' not found"
  echo ""
  echo "Available AVDs:"
  if ls "$ANDROID_SDK_ROOT/avd/"*.avd 2>/dev/null; then
    for avd_dir in "$ANDROID_SDK_ROOT/avd/"*.avd; do
      if [ -d "$avd_dir" ]; then
        basename "$avd_dir" | sed 's/.avd$//'
      fi
    done
  else
    echo "  (No AVDs found)"
  fi
  echo ""
  echo "To create this AVD, run:"
  echo "  ./create-avd.sh $AVD_NAME"
  exit 1
fi

# Check if emulator is already running
if adb devices | grep -q "emulator"; then
  echo "⚠️  Emulator is already running"
  echo "   To launch multiple emulators, they need different ports"
  echo ""
  read -p "Continue anyway? (y/N) " -n 1 -r
  echo
  if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    exit 0
  fi
fi

echo "📱 Starting emulator with punch-hole camera configuration..."
echo ""

# Set environment for emulator to find AVDs and system images in writable SDK
export ANDROID_HOME="$ANDROID_SDK_ROOT"
export ANDROID_SDK_ROOT="$ANDROID_SDK_ROOT"
export ANDROID_SDK_HOME="$ANDROID_SDK_ROOT"
export ANDROID_AVD_HOME="$ANDROID_SDK_ROOT/avd"

# Launch emulator in background with optimal settings
emulator @"$AVD_NAME" \
  -gpu host \
  -skin 1080x2400 \
  -no-snapshot-load \
  -no-boot-anim \
  -show-kernel \
  -verbose &

EMULATOR_PID=$!

echo "⏳ Waiting for emulator to boot..."
echo "   (This may take 30-60 seconds on first launch)"
echo ""

# Wait for emulator to be ready
timeout_seconds=120
elapsed=0

while ! adb shell getprop sys.boot_completed 2>/dev/null | grep -q "1"; do
  if [ $elapsed -ge $timeout_seconds ]; then
    echo "❌ Timeout waiting for emulator to boot"
    echo "   The emulator may still be starting. Check with: adb devices"
    kill $EMULATOR_PID 2>/dev/null
    exit 1
  fi

  echo -n "⏳ "
  date +"%T"
  sleep 5
  elapsed=$((elapsed + 5))

  # Check if emulator process is still running
  if ! kill -0 $EMULATOR_PID 2>/dev/null; then
    echo "❌ Emulator process died unexpectedly"
    exit 1
  fi
done

echo ""
echo "✅ Emulator booted successfully!"
echo ""

# Get emulator info
EMULATOR_DEVICE=$(adb devices | grep emulator | awk '{print $1}')
ANDROID_VERSION=$(adb shell getprop ro.build.version.release)
API_LEVEL=$(adb shell getprop ro.build.version.sdk)

echo "📱 Device Information:"
echo "   Device ID: $EMULATOR_DEVICE"
echo "   Android Version: $ANDROID_VERSION (API $API_LEVEL)"
echo ""

# Check for camera cutout support
CUTOUT_MODE=$(adb shell getprop ro.screen.camera.cutout)
if [ -n "$CUTOUT_MODE" ]; then
  echo "📷 Camera Cutout: $CUTOUT_MODE"
else
  echo "📷 Camera Cutout: Not detected in system properties"
  echo "   (Cutout simulation may still work via emulator configuration)"
fi

echo ""
echo "🎉 Emulator is ready for testing!"
echo ""
echo "To deploy and run your app:"
echo "  make android-debug    # Build the APK"
echo "  adb install build/android/app-universal-debug.apk"
echo "  adb shell am start -n xyz.waozi.inbe/.MainActivity"
echo ""
echo "To stop the emulator:"
echo "  adb emu kill"
echo "  or: kill $EMULATOR_PID"
echo ""
echo "To delete this AVD:"
echo "  rm -rf \"$ANDROID_SDK_ROOT/avd/$AVD_NAME.avd\""
