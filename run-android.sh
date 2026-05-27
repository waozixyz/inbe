#!/usr/bin/env bash

# Exit immediately if a command exits with a non-zero status
set -e

# 1. Define paths and package configuration
APK_PATH="build/android/app-universal-debug.apk"
# TODO: Replace with your actual Android package name from AndroidManifest.xml
PACKAGE_NAME="xyz.waozi.inbe"
LAUNCHER_ACTIVITY="android.app.NativeActivity"

echo "🔄 Waiting for device..."
adb wait-for-device

echo "📦 Installing universal debug APK..."
# The -r flag replaces the existing application if it's already there
adb install -r "$APK_PATH"

echo "🚀 Launching application..."
# Starts the main activity
adb shell am start -n "$PACKAGE_NAME/$LAUNCHER_ACTIVITY"

echo "✅ Done!"
