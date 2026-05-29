#!/usr/bin/env bash

set -e

AVD_NAME="${1:-inbe-test}"
DEVICE="pixel_7_pro"
# Use Android 33 with google_apis - available in Nix store
SYSTEM_IMAGE="system-images;android-33;google_apis;x86_64"
SYSTEM_IMAGE_PATH="/nix/store/gdl6fjkag55fqvwsgb6g2y9isq80s1bj-android-sdk-system-image-33-google_apis-arm64-v8a-system-image-33-google_apis-x86_64-33-google_apis-arm64-v8a-33-google_apis-x86_64/libexec/android-sdk/system-images/android-33/google_apis/x86_64"

echo "📱 Creating AVD: $AVD_NAME"

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

# Check and copy system image from Nix store
TARGET_DIR="$ANDROID_SDK_ROOT/system-images/android-33/google_apis/x86_64"

if [ ! -d "$TARGET_DIR" ]; then
  echo "📋 Copying system image..."
  mkdir -p "$TARGET_DIR"

  if [ ! -d "$SYSTEM_IMAGE_PATH" ]; then
    echo "❌ Error: System image not found in Nix store"
    exit 1
  fi

  cp -r "$SYSTEM_IMAGE_PATH"/* "$TARGET_DIR/"
fi

# Check if AVD already exists
if [ -d "$ANDROID_SDK_ROOT/avd/$AVD_NAME.avd" ]; then
  rm -rf "$ANDROID_SDK_ROOT/avd/$AVD_NAME.avd"
  rm -f "$ANDROID_SDK_ROOT/avd/$AVD_NAME.ini"
fi

# Create AVD
echo "🔨 Creating AVD..."
AVD_DIR="$ANDROID_SDK_ROOT/avd/$AVD_NAME.avd"
AVD_INI="$ANDROID_SDK_ROOT/avd/$AVD_NAME.ini"

mkdir -p "$AVD_DIR"

# Create AVD configuration
cat > "$AVD_DIR/config.ini" << EOF
AvdId=$AVD_NAME
avd.ini.encoding=UTF-8
PlayStore.enabled=false
abi.type=x86_64
hw.cpu.arch=x86_64
hw.device.name=Pixel 7 Pro
hw.lcd.density=420
hw.lcd.height=3120
hw.lcd.width=1440
hw.gpu.enabled=yes
hw.gpu.mode=host
hw.ramSize=2048
vm.heapSize=512
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
image.sysdir.1=system-images/android-33/google_apis/x86_64/
showDeviceFrame=yes
skin.name=1440x3120
skin.path=1440x3120
EOF

cat > "$AVD_INI" << EOF
avd.ini.encoding=UTF-8
path=$AVD_DIR
target=default
EOF

echo "✅ Done!"
