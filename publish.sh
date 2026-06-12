#!/usr/bin/env bash
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
cleanup() {
    rm -rf "$TMPDIR"
    if [ "${PACKAGE_ONLY:-0}" != "1" ]; then
        rm -f "$SITE_DIR/inbe.tar.gz"
    fi
}
trap cleanup EXIT

# Base site files. Keep this as an allowlist so source/build inputs never leak
# into the published archive.
for site_file in \
    index.html \
    builds.html \
    privacy.html \
    legacy-converter.html \
    style.css \
    og.png \
    og.jpg \
    sitemap.xml \
    robots.txt \
    manifest.json; do
    [ -f "$site_file" ] && cp "$site_file" "$TMPDIR/"
done

[ -d "web-assets" ] && rsync -a web-assets/ "$TMPDIR/web-assets/"

mkdir -p "$TMPDIR/uxn/devices"
[ -f "uxn/index.html" ] && cp uxn/index.html "$TMPDIR/uxn/"
[ -f "uxn/inbe.rom" ] && cp uxn/inbe.rom "$TMPDIR/uxn/"
if [ -d "uxn/devices" ]; then
    rsync -av --exclude='*.sym' uxn/devices/*.js "$TMPDIR/uxn/devices/" 2>/dev/null || true
fi

# Raylib builds (primary implementation)
mkdir -p "$TMPDIR/build/linux" "$TMPDIR/build/windows" "$TMPDIR/build/android"
if [ -d "build/web" ]; then
    mkdir -p "$TMPDIR/build/web"
    for raylib_web_artifact in build/web/index.html build/web/index.js build/web/index.wasm build/web/index.data; do
        [ -f "$raylib_web_artifact" ] && cp "$raylib_web_artifact" "$TMPDIR/build/web/"
    done
fi
if [ -d "build/bin/linux" ] || [ -d "build/dist/linux" ]; then
    for raylib_linux_artifact in build/bin/linux/inbe-linux-* build/dist/linux/inbe-linux.tar.gz; do
        [ -f "$raylib_linux_artifact" ] && cp "$raylib_linux_artifact" "$TMPDIR/build/linux/"
    done
fi
if [ -d "build/bin/windows" ] || [ -d "build/dist/windows" ]; then
    for raylib_windows_artifact in build/bin/windows/inbe-windows-*.exe build/dist/windows/inbe-windows.zip; do
        [ -f "$raylib_windows_artifact" ] && cp "$raylib_windows_artifact" "$TMPDIR/build/windows/"
    done
fi
if [ -d "build/android" ]; then
    for raylib_android_artifact in build/android/inbe-*.apk build/android/inbe-latest.apk build/android/app-universal-release.apk; do
        [ -f "$raylib_android_artifact" ] && cp "$raylib_android_artifact" "$TMPDIR/build/android/"
    done
fi

mkdir -p "$TMPDIR/wasm4/build"
[ -f "wasm4/build/index.html" ] && cp wasm4/build/index.html "$TMPDIR/wasm4/build/"
[ -f "wasm4/build/inbe.wasm" ] && cp wasm4/build/inbe.wasm "$TMPDIR/wasm4/build/"
for wasm4_artifact in wasm4/build/inbe-x86_64 wasm4/build/inbe-x86_64.exe wasm4/build/inbe-windows.zip wasm4/build/inbe-linux.tar.gz; do
    [ -f "$wasm4_artifact" ] && cp "$wasm4_artifact" "$TMPDIR/wasm4/build/"
done
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
[ -f "build/android/inbe-latest.apk" ] && cp build/android/inbe-latest.apk "$TMPDIR/droid/app-release.apk"

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

if [ "${PACKAGE_ONLY:-0}" = "1" ]; then
    echo "✓ Package created at $SITE_DIR/inbe.tar.gz"
    exit 0
fi

hut pages publish -d inbe.waozi.xyz "$SITE_DIR/inbe.tar.gz"
echo "✓ Published to https://inbe.waozi.xyz"

echo "✓ Done"
