#!/usr/bin/env bash
# Copy assets to Android project without requiring nix-shell or Makefile dependencies
# This script is used by GitHub Actions and F-Droid builds
# This script works from any directory by detecting the repository root

set -e

# Find the repository root by locating this script's directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"

# Change to repository root so all relative paths work correctly
cd "$REPO_ROOT" || exit 1

echo "Running from repository root: $(pwd)"
echo "Copying assets to Android project..."

ANDROID_DIR="droid"
ASSETS_DIR="$ANDROID_DIR/app/src/main/assets"

echo "Copying assets to Android project..."

# Remove and recreate assets directory
rm -rf "$ASSETS_DIR"
mkdir -p "$ASSETS_DIR"

# Copy config files
echo "Copying config files..."
cp inbe.ini "$ASSETS_DIR/inbe.ini"
cp theme.ini "$ASSETS_DIR/theme.ini"

# Copy locales
echo "Copying locales..."
mkdir -p "$ASSETS_DIR/locales"
cp locales/*.txt "$ASSETS_DIR/locales/"

# Copy themes
echo "Copying themes..."
mkdir -p "$ASSETS_DIR/themes"
cp vendor/flint/themes/*.ini "$ASSETS_DIR/themes/" 2>/dev/null || echo "No theme files found in vendor/flint/themes/"

# Copy images
echo "Copying images..."
mkdir -p "$ASSETS_DIR/assets"
cp assets/angel.jpg assets/begin.jpg "$ASSETS_DIR/assets/"

# Copy sounds
echo "Copying sounds..."
mkdir -p "$ASSETS_DIR/assets/sounds"
cp assets/sounds/breath-in.ogg assets/sounds/breath-out.ogg assets/sounds/bell.ogg "$ASSETS_DIR/assets/sounds/"

# Build and copy fonts
echo "Building fonts..."
mkdir -p assets/fonts
make -C ../otfchop otfchop
../otfchop/otfchop ../otfchop/unifont-17.0.04.otf locales/*.txt assets/fonts/locales

echo "Copying fonts..."
mkdir -p "$ASSETS_DIR/assets/fonts"
cp assets/fonts/locales.png assets/fonts/locales.dat "$ASSETS_DIR/assets/fonts/"

echo "Assets copied successfully!"
