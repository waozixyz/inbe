#!/bin/bash
# Publish inbe HTML5 to itch.io using butler

set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

# Check if butler is installed
if ! command -v butler &> /dev/null; then
    echo "Error: 'butler' command not found"
    echo "Install it from: https://itch.io/docs/butler/"
    exit 1
fi

WEB_BUILD_DIR="build/web"

# Verify the web build exists
if [ ! -f "$WEB_BUILD_DIR/index.html" ]; then
    echo "Error: Web build not found at $WEB_BUILD_DIR/index.html"
    echo "Run 'make web' first."
    exit 1
fi

# Read version from src/version.h
VERSION=$(grep INBE_VERSION_STRING src/version.h | grep -o '"[^"]*"' | tr -d '"')
if [ -z "$VERSION" ]; then
    echo "Error: Could not determine version from src/version.h"
    exit 1
fi

ITCH_USER="waozi"
ITCH_GAME="inbe"
ITCH_CHANNEL="html5"

echo "Publishing inbe v$VERSION ($ITCH_CHANNEL) to itch.io..."
echo "  User: $ITCH_USER"
echo "  Game: $ITCH_GAME"
echo "  Channel: $ITCH_CHANNEL"
echo "  Build dir: $WEB_BUILD_DIR"
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

butler push "$TMPDIR" "$ITCH_USER/$ITCH_GAME:$ITCH_CHANNEL" --userversion "$VERSION"

echo "✓ Published inbe v$VERSION to https://$ITCH_USER.itch.io/$ITCH_GAME"
