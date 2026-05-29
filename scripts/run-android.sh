#!/usr/bin/env bash

set -e

APK_PATH="build/android/app-universal-debug.apk"
PACKAGE_NAME="xyz.waozi.inbe"

echo "🚀 Android App Builder & Installer"
echo "   This script builds the APK and installs it to your chosen device"
echo ""

# Build APK
echo "🔨 Building APK..."
make android-debug
echo "✅ APK built: $APK_PATH"
echo ""

# Get list of connected devices
mapfile -t DEVICES < <(adb devices | grep -v "List" | grep -v "^$" | awk '{print $1}')

if [ ${#DEVICES[@]} -eq 0 ]; then
  echo "❌ No Android devices found"
  echo "   Connect a device or start an emulator:"
  echo "   ./scripts/emulator.sh"
  exit 1
fi

echo "📱 Found ${#DEVICES[@]} device(s):"
for i in "${!DEVICES[@]}"; do
  DEVICE_ID="${DEVICES[$i]}"
  # Get device info
  DEVICE_NAME=$(adb -s "$DEVICE_ID" shell getprop ro.product.model 2>/dev/null | tr -d '\r')
  DEVICE_ANDROID=$(adb -s "$DEVICE_ID" shell getprop ro.build.version.release 2>/dev/null | tr -d '\r')
  echo "   [$i] $DEVICE_ID"
  echo "       Model: $DEVICE_NAME"
  echo "       Android: $DEVICE_ANDROID"
  echo ""
done

# Select device
if [ ${#DEVICES[@]} -eq 1 ]; then
  SELECTED_DEVICE="${DEVICES[0]}"
  echo "✅ Using only available device"
else
  echo -n "👉 Select device number [0-$((${#DEVICES[@]}-1))]: "
  read -r SELECTION
  SELECTED_DEVICE="${DEVICES[$SELECTION]}"
fi

echo ""
echo "📦 Installing APK to $SELECTED_DEVICE..."
adb -s "$SELECTED_DEVICE" install -r "$APK_PATH"

echo "🚀 Launching app..."
adb -s "$SELECTED_DEVICE" shell am start -n "$PACKAGE_NAME/.MainActivity"

echo ""
echo "🎉 App is running!"
echo ""
echo "To rebuild and reinstall:"
echo "  ./scripts/run-android.sh"
echo ""
echo "To view logs:"
echo "  adb -s $SELECTED_DEVICE logcat | grep inbe"
