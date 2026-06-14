#!/usr/bin/env bash
# Generate embedded assets for Android without requiring nix-shell.
# This script is used by GitHub Actions and F-Droid builds
# This script works from any directory by detecting the repository root

set -e

# Find the repository root by locating this script's directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"

# Change to repository root so all relative paths work correctly
cd "$REPO_ROOT" || exit 1

echo "Running from repository root: $(pwd)"
echo "Generating embedded assets for Android..."

ANDROID_DIR="droid"
ASSETS_DIR="$ANDROID_DIR/app/src/main/assets"

# Keep APK assets empty; runtime assets are compiled into the native library.
rm -rf "$ASSETS_DIR"
mkdir -p "$ASSETS_DIR"

# Build fonts only when explicitly requested. Android/F-Droid builds use the
# committed glyph atlas so they do not need host build tools beyond Gradle.
if [ "${INBE_REBUILD_FONTS:-0}" = "1" ]; then
    echo "Building fonts..."
    mkdir -p assets/fonts
    make -C vendor/otfchop otfchop
    vendor/otfchop/otfchop vendor/otfchop/unifont-17.0.04.otf locales/*.txt assets/fonts/locales
else
    echo "Using versioned fonts..."
fi

mkdir -p build
sh flint/scripts/embed-assets.sh build/inbe_embedded_assets.c \
    locales/*.txt \
    assets/whm/1.jpg \
    assets/whm/2.jpg \
    assets/fonts/locales.png \
    assets/fonts/locales.dat \
    assets/sounds/*.ogg

echo "Embedded assets generated successfully!"
