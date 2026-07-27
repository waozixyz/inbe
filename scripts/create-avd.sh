#!/usr/bin/env bash

set -e

AVD_NAME="${1:-inbe-test}"

echo "📱 Creating AVD: $AVD_NAME (Pixel 8 Pro - Android API ${ANDROID_API:-35}, punch-hole test profile)"

# Check for Android SDK location
if [ -z "$ANDROID_SDK_ROOT" ] && [ -z "$ANDROID_HOME" ]; then
  echo "❌ Error: Neither ANDROID_SDK_ROOT nor ANDROID_HOME is set"
  echo "   Install the Android SDK and export ANDROID_SDK_ROOT or ANDROID_HOME"
  exit 1
fi

UNAME_S="$(uname -s)"

# Set up persistent writable SDK location. Android's Linux host tools cannot
# reliably see FreeBSD home paths through linuxulator, so keep their working SDK
# under /tmp unless the caller provides a specific location.
ORIGINAL_SDK_ROOT="${ANDROID_SDK_ROOT:-$ANDROID_HOME}"
if [ "$UNAME_S" = "FreeBSD" ]; then
  PERSISTENT_SDK_ROOT="${ANDROID_SDK_WORK_ROOT:-/tmp/android-sdk}"
else
  PERSISTENT_SDK_ROOT="${ANDROID_SDK_WORK_ROOT:-$HOME/.android-sdk-writable}"
fi
export ANDROID_SDK_ROOT="$PERSISTENT_SDK_ROOT"
export ANDROID_HOME="$PERSISTENT_SDK_ROOT"

mkdir -p "$ANDROID_SDK_ROOT"

# Reuse SDK components from the original SDK. On FreeBSD, copy them because the
# Linux binaries may not be able to follow symlinks back into /home.
for component in build-tools cmake cmdline-tools emulator licenses ndk platforms platform-tools tools; do
  if [ -e "$ORIGINAL_SDK_ROOT/$component" ] && [ ! -e "$ANDROID_SDK_ROOT/$component" ]; then
    if [ "$UNAME_S" = "FreeBSD" ]; then
      cp -R "$ORIGINAL_SDK_ROOT/$component" "$ANDROID_SDK_ROOT/$component"
      chmod -R u+w "$ANDROID_SDK_ROOT/$component" 2>/dev/null || true
    else
      ln -sf "$ORIGINAL_SDK_ROOT/$component" "$ANDROID_SDK_ROOT/$component"
    fi
  fi
done

mkdir -p "$ANDROID_SDK_ROOT/system-images"
mkdir -p "$ANDROID_SDK_ROOT/avd"

ANDROID_API="${ANDROID_API:-35}"
SYSTEM_IMAGE_TYPE=""
ABI_TYPE=""
SOURCE_DIR=""
TARGET_DIR=""

image_dir_ready() {
  dir="$1"
  [ -d "$dir" ] && { [ -f "$dir/source.properties" ] || [ -f "$dir/system.img" ] || [ -f "$dir/kernel-ranchu" ]; }
}

find_system_image() {
  for spec in \
    "google_apis_playstore_ps16k x86_64" \
    "google_apis_ps16k x86_64" \
    "google_apis_playstore x86_64" \
    "google_apis x86_64" \
    "default x86_64" \
    "google_apis_playstore_ps16k arm64-v8a" \
    "google_apis_ps16k arm64-v8a" \
    "google_apis_playstore arm64-v8a" \
    "google_apis arm64-v8a" \
    "default arm64-v8a"; do
    set -- $spec
    type="$1"
    abi="$2"
    for root in "$ANDROID_SDK_ROOT" "$ORIGINAL_SDK_ROOT"; do
      dir="$root/system-images/android-$ANDROID_API/$type/$abi"
      if image_dir_ready "$dir"; then
        SYSTEM_IMAGE_TYPE="$type"
        ABI_TYPE="$abi"
        SOURCE_DIR="$dir"
        TARGET_DIR="$ANDROID_SDK_ROOT/system-images/android-$ANDROID_API/$type/$abi"
        return 0
      fi
    done
  done
  return 1
}

install_system_image() {
  if command -v sdkmanager >/dev/null 2>&1; then
    SDKMANAGER=sdkmanager
  elif [ -x "$ANDROID_SDK_ROOT/cmdline-tools/11.0/bin/sdkmanager" ]; then
    SDKMANAGER="$ANDROID_SDK_ROOT/cmdline-tools/11.0/bin/sdkmanager"
  elif [ -x "$ANDROID_SDK_ROOT/cmdline-tools/latest/bin/sdkmanager" ]; then
    SDKMANAGER="$ANDROID_SDK_ROOT/cmdline-tools/latest/bin/sdkmanager"
  else
    echo "❌ Error: sdkmanager not found"
    return 1
  fi

  for spec in \
    "google_apis_playstore_ps16k x86_64" \
    "google_apis_ps16k x86_64" \
    "google_apis_playstore x86_64" \
    "google_apis x86_64" \
    "default x86_64" \
    "google_apis_playstore_ps16k arm64-v8a" \
    "google_apis_ps16k arm64-v8a" \
    "google_apis_playstore arm64-v8a" \
    "google_apis arm64-v8a" \
    "default arm64-v8a"; do
    set -- $spec
    type="$1"
    abi="$2"
    package="system-images;android-$ANDROID_API;$type;$abi"
    echo "📦 Installing Android system image: $package"
    if yes | "$SDKMANAGER" --sdk_root="$ANDROID_SDK_ROOT" "$package"; then
      if find_system_image; then
        return 0
      fi
    fi
  done

  return 1
}

if ! find_system_image; then
  echo "ℹ️  Android $ANDROID_API system image not found locally. Installing one now..."
  if ! install_system_image; then
    echo "❌ Error: Could not install an Android $ANDROID_API emulator system image"
    exit 1
  fi
fi

if [ "$SOURCE_DIR" != "$TARGET_DIR" ]; then
  echo "📋 Copying system image ($SYSTEM_IMAGE_TYPE/$ABI_TYPE)..."
  if [ -e "$TARGET_DIR" ]; then
    chmod -R u+w "$TARGET_DIR" 2>/dev/null || true
    rm -rf "$TARGET_DIR"
  fi
  mkdir -p "$TARGET_DIR"
  cp -R "$SOURCE_DIR"/. "$TARGET_DIR"/
  chmod -R u+w "$TARGET_DIR" 2>/dev/null || true
fi

ABI_DISPLAY="$ABI_TYPE"
PLAYSTORE_ENABLED=false
echo "📋 Using system image: android-$ANDROID_API/$SYSTEM_IMAGE_TYPE/$ABI_TYPE"

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
PlayStore.enabled=$PLAYSTORE_ENABLED
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
image.sysdir.1=system-images/android-$ANDROID_API/$SYSTEM_IMAGE_TYPE/$ABI_TYPE/
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
echo "   Android API: $ANDROID_API"
echo "   ABI: $ABI_DISPLAY"
echo "   Cutout: punch-hole overlay enabled by scripts/emulator.sh after boot"
echo ""
echo "   Run with: bash scripts/emulator.sh"
