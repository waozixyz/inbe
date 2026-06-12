#!/bin/bash
# Publish inbe builds to itch.io using butler

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

# Check if butler is installed
if ! command -v butler &> /dev/null; then
    echo "Error: 'butler' command not found"
    echo "Install it from: https://itch.io/docs/butler/"
    exit 1
fi

# Read version from src/version.h
VERSION=$(sed -n 's/.*INBE_VERSION_STRING[[:space:]]*"\([^"]*\)".*/\1/p' src/version.h | head -n 1)
if [ -z "$VERSION" ]; then
    echo "Error: Could not determine version from src/version.h"
    exit 1
fi

ITCH_USER="waozi"
ITCH_GAME="inbe"
WEB_CHANNEL="html5"
WINDOWS_CHANNEL="windows"
LINUX_CHANNEL="linux"
ANDROID_CHANNEL="android"

WEB_BUILD_DIR="build/web"
WINDOWS_ARTIFACT="build/dist/windows/inbe-windows.zip"
LINUX_ARTIFACT="build/dist/linux/inbe-linux.tar.gz"

find_android_artifact() {
    if [ -f "build/android/inbe-$VERSION.apk" ]; then
        printf '%s\n' "build/android/inbe-$VERSION.apk"
    elif [ -e "build/android/inbe-latest.apk" ]; then
        printf '%s\n' "build/android/inbe-latest.apk"
    elif [ -d "build/android" ]; then
        find build/android -maxdepth 1 -type f \
            \( -name 'inbe-*.apk' -o -name '*-release.apk' \) \
            ! -name '*debug*' \
            | sort -V \
            | tail -n 1
    fi
}

ANDROID_ARTIFACT=$(find_android_artifact)

require_file() {
    local path="$1"
    local label="$2"
    local build_hint="$3"

    if [ ! -f "$path" ]; then
        echo "Error: $label not found at $path"
        echo "Run '$build_hint' first."
        exit 1
    fi
}

require_file "$WEB_BUILD_DIR/index.html" "Web build" "make web"
require_file "$WINDOWS_ARTIFACT" "Windows package" "make dist-windows"
require_file "$LINUX_ARTIFACT" "Linux package" "make dist-linux"
require_file "$ANDROID_ARTIFACT" "Signed Android APK" "make android-release"

push_channel() {
    local artifact="$1"
    local channel="$2"

    echo "Publishing $artifact to $ITCH_USER/$ITCH_GAME:$channel..."
    butler push "$artifact" "$ITCH_USER/$ITCH_GAME:$channel" --userversion "$VERSION"
}

echo "Publishing inbe v$VERSION to itch.io..."
echo "  User: $ITCH_USER"
echo "  Game: $ITCH_GAME"
echo "  Web channel: $WEB_CHANNEL ($WEB_BUILD_DIR)"
echo "  Windows channel: $WINDOWS_CHANNEL ($WINDOWS_ARTIFACT)"
echo "  Linux channel: $LINUX_CHANNEL ($LINUX_ARTIFACT)"
echo "  Android channel: $ANDROID_CHANNEL ($ANDROID_ARTIFACT)"
echo ""

TMPDIR=$(mktemp -d -p /tmp)
cleanup() {
    rm -rf "$TMPDIR"
}
trap cleanup EXIT

rsync -a \
    --exclude='raylib' \
    "$WEB_BUILD_DIR/" "$TMPDIR/"
rsync -a web-assets/ "$TMPDIR/web-assets/"
cp manifest.json "$TMPDIR/"

push_channel "$TMPDIR" "$WEB_CHANNEL"
push_channel "$WINDOWS_ARTIFACT" "$WINDOWS_CHANNEL"
push_channel "$LINUX_ARTIFACT" "$LINUX_CHANNEL"
push_channel "$ANDROID_ARTIFACT" "$ANDROID_CHANNEL"

echo "Published inbe v$VERSION to https://$ITCH_USER.itch.io/$ITCH_GAME"
