#!/usr/bin/env bash
# update_metadata.sh - Simple version update for website
# This runs automatically before publish to keep version info current

set -e

# Extract version from CHANGELOG.md
get_version() {
    if [ -f "CHANGELOG.md" ]; then
        grep '^## \[' CHANGELOG.md | head -n 1 | sed 's/^## \[\([^]]*\)\].*/\1/'
    else
        echo "unknown"
    fi
}

echo "=== Updating Build Metadata ==="

# Get current version
VERSION=$(get_version)
echo "Current version: $VERSION"

# Update index.html
if [ -f "index.html" ]; then
    echo "Updating index.html..."

    # Create backup
    cp index.html index.html.bak

    # Update version placeholder
    sed -i "s|VERSION_PLACEHOLDER|$VERSION|g" index.html

    echo "✓ index.html updated with version $VERSION"
else
    echo "Warning: index.html not found"
fi

# Update builds.html if it exists
if [ -f "builds.html" ]; then
    echo "Updating builds.html..."

    # Create backup
    cp builds.html builds.html.bak

    # Update version placeholder
    sed -i "s|VERSION_PLACEHOLDER|$VERSION|g" builds.html

    echo "✓ builds.html updated with version $VERSION"
else
    echo "builds.html not found, skipping"
fi

echo "=== Metadata Update Complete ==="