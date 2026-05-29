#!/usr/bin/env bash

set -e

AVD_NAME="${1:-inbe-test}"
SYSTEM_IMAGE="system-images;android-35;google_apis;x86_64"
ABI="google_apis;x86_64"
PLATFORM="android-35"

echo "🔧 Creating Android Virtual Device for camera notch testing"
echo "   AVD Name: $AVD_NAME"
echo "   System Image: Android 35 (x86_64 with Google APIs)"
echo ""

# Check for ANDROID_HOME
if [ -z "$ANDROID_HOME" ]; then
  echo "❌ Error: ANDROID_HOME environment variable not set"
  echo "   Make sure to run this script within 'nix develop'"
  exit 1
fi

# Check for required tools
if ! command -v avdmanager &> /dev/null; then
  echo "❌ Error: avdmanager not found in PATH"
  echo "   Make sure android-emulator is installed in your nix environment"
  exit 1
fi

if ! command -v sdkmanager &> /dev/null; then
  echo "❌ Error: sdkmanager not found in PATH"
  echo "   Make sure cmdline-tools are installed in your nix environment"
  exit 1
fi

# Set up writable SDK locations for downloads and AVDs
# Nix store is read-only, so we use the system SDK for writable content
WRITABLE_SDK="/mnt/storage/Android/Sdk"
if [ -d "$WRITABLE_SDK" ] && [ -w "$WRITABLE_SDK" ]; then
  export ANDROID_SDK_ROOT="$WRITABLE_SDK"
  echo "📂 Using writable Android SDK: $ANDROID_SDK_ROOT"
else
  echo "❌ Error: No writable Android SDK location found"
  echo "   Nix Android SDK is read-only"
  exit 1
fi

# Check if system image is installed locally
echo "🔍 Checking for system image..."
SYSTEM_IMAGE_DIR="$ANDROID_SDK_ROOT/system-images/android-35/google_apis/x86_64"

if [ -d "$SYSTEM_IMAGE_DIR" ]; then
  echo "✅ System image already installed"
else
  echo "📦 System image not found locally. Downloading Android 35 system image..."
  echo "   This is a large download (~1GB) and may take 5-15 minutes..."
  echo "   Downloading to: $ANDROID_SDK_ROOT"
  echo "   Progress will be shown below:"

  # Use ANDROID_SDK_ROOT environment variable for sdkmanager
  if ANDROID_SDK_ROOT="$ANDROID_SDK_ROOT" sdkmanager "system-images;android-35;google_apis;x86_64"; then
    echo ""
    echo "✅ System image downloaded successfully"
  else
    echo "❌ Failed to download system image"
    echo "   You may need to accept licenses first: yes | sdkmanager --licenses"
    exit 1
  fi
fi

# Create AVD manually since avdmanager doesn't properly support writable SDK locations
echo ""
echo "🎯 Creating AVD configuration..."
echo "Creating AVD directory and configuration files..."

# Check if AVD already exists
if [ -d "$ANDROID_SDK_ROOT/avd/$AVD_NAME.avd" ]; then
  echo "⚠️  AVD '$AVD_NAME' already exists"
  echo "   Delete it first with: rm -rf \"$ANDROID_SDK_ROOT/avd/$AVD_NAME.avd\""
  echo "   Or use a different name: $0 <new-name>"
  exit 1
fi

# Create AVD directory
mkdir -p "$ANDROID_SDK_ROOT/avd/$AVD_NAME.avd"

# Create the AVD configuration files manually
cat > "$ANDROID_SDK_ROOT/avd/$AVD_NAME.ini" <<EOF
avd.ini.encoding=UTF-8
path=$ANDROID_SDK_ROOT/avd/$AVD_NAME.avd
path.rel=avd/$AVD_NAME.avd
target=android-35
EOF

# Create the AVD hardware configuration with punch-hole camera
cat > "$ANDROID_SDK_ROOT/avd/$AVD_NAME.avd/config.ini" <<EOF
AvdId=$AVD_NAME
PlayStore.enabled=false
abi.type=x86_64
hw.cpu.arch=x86_64
hw.audioInput=yes
hw.audioOutput=yes
hw.battery=yes
hw.camera.back=emulated
hw.camera.front=emulated
hw.cpu.ncore=4
hw.dPad=no
hw.device.hash2=MD5:1c04b2aa69625b0d6da3180df3f10859
hw.device.manufacturer=Google
hw.device.name=pixel_6
hw.gps=yes
hw.gpu.enabled=yes
hw.gpu.mode=host
hw.initialOrientation=Portrait
hw.keyboard=yes
hw.lcd.density=420
hw.lcd.height=2400
hw.lcd.width=1080
hw.mainKeys=no
hw.ramSize=4096
hw.sensor.accelerometer=yes
hw.sensor.compass=yes
hw.sensor.gyroscope=yes
hw.sensor.light=yes
hw.sensor.proximity=yes
hw.touchScreen=yes
disk.cachePartition=yes
disk.cachePartition.size=800MB
disk.dataPartition.size=800MB
fastboot.forceColdBoot=no
fastboot.forceFastBoot=yes
fastboot.forceChosenSnapshotBoot=no
hw.useDigitizer=no
tag.id=google_apis
tag.display=Google APIs
image.sysdir.1=system-images/android-35/google_apis/x86_64/
EOF

# Check if AVD was created successfully
if [ -f "$ANDROID_SDK_ROOT/avd/$AVD_NAME.avd/config.ini" ]; then
  echo "✅ AVD configured with punch-hole camera profile"
else
  echo "❌ Failed to create AVD configuration"
  exit 1
fi

echo ""
echo "📱 AVD Configuration:"
echo "   Device: Pixel 6 (punch-hole camera)"
echo "   Display: 1080x2400 @ 420dpi"
echo "   RAM: 4096MB"
echo "   GPU: Hardware acceleration enabled"
echo ""
echo "🎉 AVD '$AVD_NAME' created successfully!"
echo "   Location: $ANDROID_SDK_ROOT/avd/$AVD_NAME.avd"
echo ""
echo "To launch the emulator, run:"
echo "  ./scripts/launch-emulator.sh $AVD_NAME"
echo ""
echo "Or to launch with a custom name:"
echo "  ./scripts/launch-emulator.sh <avd-name>"
