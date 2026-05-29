#!/usr/bin/env bash

set -e

AVD_NAME="${1:-inbe-test}"

echo "📱 Creating AVD: $AVD_NAME (Pixel 8 Pro - Android 14)"

# Check for Android SDK location
if [ -z "$ANDROID_SDK_ROOT" ] && [ -z "$ANDROID_HOME" ]; then
  echo "❌ Error: Neither ANDROID_SDK_ROOT nor ANDROID_HOME is set"
  echo "   Make sure to run this script within 'nix develop'"
  exit 1
fi

# Set up persistent writable SDK location
ORIGINAL_SDK_ROOT="$ANDROID_SDK_ROOT"
PERSISTENT_SDK_ROOT="$HOME/.android-sdk-writable"
export ANDROID_SDK_ROOT="$PERSISTENT_SDK_ROOT"
export ANDROID_HOME="$PERSISTENT_SDK_ROOT"

mkdir -p "$ANDROID_SDK_ROOT"

# Create symlinks for SDK components
for component in build-tools cmake cmdline-tools emulator licenses ndk platforms platform-tools tools; do
  if [ -e "$ORIGINAL_SDK_ROOT/$component" ] && [ ! -e "$ANDROID_SDK_ROOT/$component" ]; then
    ln -sf "$ORIGINAL_SDK_ROOT/$component" "$ANDROID_SDK_ROOT/$component"
  fi
done

mkdir -p "$ANDROID_SDK_ROOT/system-images"
mkdir -p "$ANDROID_SDK_ROOT/avd"

# Determine system image paths
# Prefer x86_64 for faster emulation
TARGET_DIR="$ANDROID_SDK_ROOT/system-images/android-34/google_apis_playstore/x86_64"
ABI_TYPE="x86_64"
ABI_DISPLAY="x86_64"

# Find system image in Nix store
SYSTEM_IMAGE_BASE=$(find "$ORIGINAL_SDK_ROOT/../.." -maxdepth 1 -type d -name "*system-image*34*google_apis_playstore*" | head -1)

if [ -z "$SYSTEM_IMAGE_BASE" ] || [ ! -d "$SYSTEM_IMAGE_BASE" ]; then
  echo "❌ Error: System image not found in Nix store"
  echo "   Searched for: *system-image*34*google_apis_playstore* in $ORIGINAL_SDK_ROOT/../.."
  exit 1
fi

echo "📋 Found system image base: $SYSTEM_IMAGE_BASE"

# Check and copy system image from Nix store
if [ ! -d "$TARGET_DIR" ]; then
  echo "📋 Copying system image ($ABI_TYPE)..."

  SOURCE_DIR="$SYSTEM_IMAGE_BASE/libexec/android-sdk/system-images/android-34/google_apis_playstore/$ABI_TYPE"

  if [ ! -d "$SOURCE_DIR" ]; then
    echo "❌ Error: Source directory not found: $SOURCE_DIR"
    exit 1
  fi

  mkdir -p "$TARGET_DIR"
  cp -r "$SOURCE_DIR"/* "$TARGET_DIR/"
  echo "✅ Copied $ABI_TYPE system image"
fi

# Check if AVD already exists
if [ -d "$ANDROID_SDK_ROOT/avd/$AVD_NAME.avd" ]; then
  echo "🗑️  Removing existing AVD..."
  rm -rf "$ANDROID_SDK_ROOT/avd/$AVD_NAME.avd"
  rm -f "$ANDROID_SDK_ROOT/avd/$AVD_NAME.ini"
fi

# Create AVD
echo "🔨 Creating AVD ($ABI_DISPLAY)..."
AVD_DIR="$ANDROID_SDK_ROOT/avd/$AVD_NAME.avd"
AVD_INI="$ANDROID_SDK_ROOT/avd/$AVD_NAME.ini"

mkdir -p "$AVD_DIR"

# Pixel 8 Pro specs: 2960 x 1440, 6.7", 512 DPI, 12GB RAM
cat > "$AVD_DIR/config.ini" << EOF
AvdId=$AVD_NAME
avd.ini.encoding=UTF-8
PlayStore.enabled=true
abi.type=$ABI_TYPE
hw.cpu.arch=$ABI_TYPE
hw.device.name=Pixel 8 Pro
hw.lcd.density=512
hw.lcd.height=2960
hw.lcd.width=1440
hw.gpu.enabled=yes
hw.gpu.mode=host
hw.ramSize=4096
vm.heapSize=1024
hw.sdCard=yes
hw.sdCard.size=2048MB
disk.cachePartition=yes
disk.cachePartition.size=800MB
hw.accelerometer=yes
hw.audioInput=yes
hw.battery=yes
hw.camera.back=yes
hw.camera.front=yes
hw.gps=yes
hw.gsmModem=yes
hw.keyboard=yes
hw.touchScreen=yes
image.sysdir.1=system-images/android-34/google_apis_playstore/$ABI_TYPE/
showDeviceFrame=yes
skin.name=1440x2960
skin.path=1440x2960
EOF

cat > "$AVD_INI" << EOF
avd.ini.encoding=UTF-8
path=$AVD_DIR
target=default
EOF

echo "✅ Done!"
echo "   Device: Pixel 8 Pro"
echo "   Android: 14 (API 34)"
echo "   ABI: $ABI_DISPLAY"
echo ""
echo "   Run with: bash scripts/emulator.sh"
