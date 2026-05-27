#!/bin/bash
# Publish inbe site to inbe.waozi.xyz

set -e
SITE_DIR="$(cd "$(dirname "$0")" && pwd)"

cd "$SITE_DIR"

# Check if hut is installed
if ! command -v hut &> /dev/null; then
    echo "Error: 'hut' command not found"
    echo "Install it with: nix-shell -p hut"
    exit 1
fi

echo "Publishing inbe.waozi.xyz..."

TMPDIR=$(mktemp -d -p /tmp)

# Base site files
rsync -av --delete \
    --exclude='local.properties' \
    --exclude='*.sym' \
    --exclude='*.o' \
    --exclude='*.a' \
    --exclude='*.d' \
    --exclude='*.9' \
    --exclude='*.8' \
    --exclude='build' \
    --exclude='.gradle' \
    --exclude='.idea' \
    --exclude='node_modules' \
    --exclude='*.iml' \
    --exclude='.cxx' \
    --exclude='.externalNativeBuild' \
    --exclude='Captures' \
    --exclude='.DS_Store' \
    --exclude='Thumbs.db' \
    --exclude='.xdp-*' \
    --exclude='*.bak' \
    --exclude='*.swp' \
    --exclude='*.lock' \
    --exclude='gradlew' \
    --exclude='gradlew.bat' \
    --exclude='shell.nix' \
    --exclude='flake.nix' \
    --exclude='flake.lock' \
    --exclude='Makefile' \
    --exclude='gradle' \
    --exclude='codex' \
    --exclude='.codex' \
    --exclude='.npm' \
    --exclude='.local' \
    . "$TMPDIR/"

mkdir -p "$TMPDIR/rc"
[ -f "rc/inbe.rc" ] && cp rc/inbe.rc "$TMPDIR/rc/"

mkdir -p "$TMPDIR/uxn/devices"
[ -f "uxn/index.html" ] && cp uxn/index.html "$TMPDIR/uxn/"
[ -f "uxn/inbe.rom" ] && cp uxn/inbe.* "$TMPDIR/uxn/"
if [ -f "../uxn/devices/system.js" ]; then
    rsync -av --exclude='*.sym' ../uxn/devices/*.js "$TMPDIR/uxn/devices/"
fi
if [ -f "../uxn/uxn.js" ]; then
    rsync -av --exclude='*.sym' ../uxn/*.js "$TMPDIR/uxn/" 2>/dev/null || true
fi

# Raylib builds (primary implementation)
mkdir -p "$TMPDIR/build/linux" "$TMPDIR/build/windows" "$TMPDIR/build/android"
if [ -d "build/web" ]; then
    mkdir -p "$TMPDIR/build/web"
    for raylib_web_artifact in build/web/index.html build/web/index.js build/web/index.wasm build/web/index.data; do
        [ -f "$raylib_web_artifact" ] && cp "$raylib_web_artifact" "$TMPDIR/build/web/"
    done
fi
if [ -d "build/linux" ]; then
    for raylib_linux_artifact in build/linux/inbe-linux-* build/linux/inbe-linux.tar.gz; do
        [ -f "$raylib_linux_artifact" ] && cp "$raylib_linux_artifact" "$TMPDIR/build/linux/"
    done
fi
if [ -d "build/windows" ]; then
    for raylib_windows_artifact in build/windows/inbe-windows-* build/windows/inbe-windows.zip; do
        [ -f "$raylib_windows_artifact" ] && cp "$raylib_windows_artifact" "$TMPDIR/build/windows/"
    done
fi
if [ -d "build/android" ]; then
    for raylib_android_artifact in build/android/*.apk; do
        [ -f "$raylib_android_artifact" ] && cp "$raylib_android_artifact" "$TMPDIR/build/android/"
    done
fi

mkdir -p "$TMPDIR/wasm4/build"
[ -f "wasm4/build/index.html" ] && cp wasm4/build/index.html "$TMPDIR/wasm4/build/"
[ -f "wasm4/build/inbe.wasm" ] && cp wasm4/build/inbe.wasm "$TMPDIR/wasm4/build/"
for wasm4_artifact in wasm4/build/inbe-x86_64 wasm4/build/inbe-x86_64.exe wasm4/build/inbe-windows.zip wasm4/build/inbe-linux.tar.gz; do
    [ -f "$wasm4_artifact" ] && cp "$wasm4_artifact" "$TMPDIR/wasm4/build/"
done
[ -d "wasm4/src" ] && rsync -av --exclude='*.o' --exclude='*.d' --exclude='.npm' --exclude='.local' --exclude='.codex' wasm4/src/ "$TMPDIR/wasm4/src/"
[ -f "wasm4/Makefile" ] && cp wasm4/Makefile "$TMPDIR/wasm4/"
[ -f "wasm4/shell.nix" ] && cp wasm4/shell.nix "$TMPDIR/wasm4/"
[ -f "wasm4/README.md" ] && cp wasm4/README.md "$TMPDIR/wasm4/"

mkdir -p "$TMPDIR/linux/build"
if [ -d "linux/build" ]; then
    for linux_bin in linux/build/inbe-* linux/build/inbefb-* linux/build/inbe-linux.tar.gz; do
        [ -f "$linux_bin" ] && cp "$linux_bin" "$TMPDIR/linux/build/"
    done
fi

mkdir -p "$TMPDIR/love2d"
[ -f "love2d/inbe.html" ] && cp love2d/inbe.* "$TMPDIR/love2d/"
[ -d "love2d/build/inbe" ] && mkdir -p "$TMPDIR/love2d/build" && rsync -av --exclude='*.o' --exclude='*.d' --exclude='.npm' --exclude='.local' --exclude='.codex' love2d/build/inbe/ "$TMPDIR/love2d/build/inbe/"

mkdir -p "$TMPDIR/tcl"
[ -f "tcl/inbe.tcl" ] && cp tcl/inbe.tcl "$TMPDIR/tcl/"

# Compatibility copy for older Android links.
mkdir -p "$TMPDIR/droid"
[ -f "build/android/app-universal-release.apk" ] && cp build/android/app-universal-release.apk "$TMPDIR/droid/app-release.apk"

rsync -av --exclude='.*' --exclude='*.sym' --exclude='*.o' --exclude='*.a' --exclude='build' --exclude='.gradle' --exclude='.npm' --exclude='.local' --exclude='.codex' ../icons/ "$TMPDIR/icons/"
rsync -av --exclude='*.sym' --exclude='*.o' --exclude='*.a' --exclude='build' --exclude='.gradle' --exclude='.npm' --exclude='.local' --exclude='.codex' ../cursors/ "$TMPDIR/cursors/"
rsync -av --exclude='.*' --exclude='*.sym' --exclude='*.o' --exclude='*.a' --exclude='build' --exclude='.gradle' --exclude='Makefile' --exclude='.npm' --exclude='.local' --exclude='.codex' icons/ "$TMPDIR/icons/"
cp ../style.css ../assets/DepartureMono-Regular.otf inbe.css og.png "$TMPDIR/"
cp -R ../assets "$TMPDIR/"

if [ -f "$TMPDIR/index.html" ]; then
    sed -e 's|/inbe/droid/app/build/outputs/apk/release/app-release.apk|/build/android/app-universal-release.apk|g' \
        -e 's|/inbe/|/|g' \
        "$TMPDIR/index.html" > "$TMPDIR/index_.html" && mv "$TMPDIR/index_.html" "$TMPDIR/index.html"
fi

# Create tarball directly in SITE_DIR
echo "Creating inbe.tar.gz..."
tar -czf "$SITE_DIR/inbe.tar.gz" -C "$TMPDIR" .

if [ ! -f "$SITE_DIR/inbe.tar.gz" ]; then
    echo "Error: inbe.tar.gz was not created!"
    rm -rf "$TMPDIR"
    exit 1
fi

rm -rf "$TMPDIR"
hut pages publish -d inbe.waozi.xyz "$SITE_DIR/inbe.tar.gz"
echo "✓ Published to https://inbe.waozi.xyz"
rm -f "$SITE_DIR/inbe.tar.gz"

echo "✓ Done"
