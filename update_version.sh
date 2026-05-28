#!/usr/bin/env bash
# update_version.sh - Update version from CHANGELOG.md (master truth)
# Usage: ./update_version.sh

set -e

CHANGELOG_FILE="CHANGELOG.md"
GRADLE_FILE="droid/app/build.gradle"
CHANGELOG_DIR="fastlane/metadata/android/en-US/changelogs"

# Extract all versions from CHANGELOG
VERSIONS=$(grep '^## \[' "$CHANGELOG_FILE" | sed 's/^## \[\([^]]*\)\].*/\1/')

if [ -z "$VERSIONS" ]; then
    echo "Error: No versions found in $CHANGELOG_FILE"
    echo "Expected format: ## [1.0.0] - YYYY-MM-DD"
    exit 1
fi

# Get latest version for gradle update
LATEST_VERSION=$(echo "$VERSIONS" | head -n 1)

# Count total versions for versionCode (position-based: 11 versions = code 11)
VERSION_COUNT=$(echo "$VERSIONS" | wc -l)

# Get current gradle values
CURRENT_NAME=$(grep "versionName" "$GRADLE_FILE" | sed 's/.*"\([^"]*\)".*/\1/')
CURRENT_CODE=$(grep "versionCode" "$GRADLE_FILE" | awk '{print $2}')

# Only increment versionCode if versionName actually changed
# Otherwise, use the position count (in case we're fixing a broken code)
if [ "$LATEST_VERSION" = "$CURRENT_NAME" ]; then
    # Version same - if current code is wrong (too low), fix it
    if [ "$CURRENT_CODE" -lt "$VERSION_COUNT" ]; then
        VERSION_CODE=$VERSION_COUNT
    else
        VERSION_CODE=$CURRENT_CODE
    fi
else
    # New version - use max of (current+1) or (version count)
    MAX_CODE=$((CURRENT_CODE + 1))
    if [ "$VERSION_COUNT" -gt "$MAX_CODE" ]; then
        VERSION_CODE=$VERSION_COUNT
    else
        VERSION_CODE=$MAX_CODE
    fi
fi

echo "Updating to: $LATEST_VERSION (code $VERSION_CODE)"
echo "Current: $CURRENT_NAME (code $CURRENT_CODE)"

# Update gradle file with latest version
sed -i "s/versionCode $CURRENT_CODE/versionCode $VERSION_CODE/" "$GRADLE_FILE"
sed -i "s/versionName \"$CURRENT_NAME\"/versionName \"$LATEST_VERSION\"/" "$GRADLE_FILE"

echo "✓ Updated $GRADLE_FILE"

# Update version.h
VERSION_H_FILE="src/version.h"
MAJOR=$(echo "$LATEST_VERSION" | cut -d. -f1)
MINOR=$(echo "$LATEST_VERSION" | cut -d. -f2)
PATCH=$(echo "$LATEST_VERSION" | cut -d. -f3)

sed -i "s/^#define INBE_VERSION_MAJOR .*/#define INBE_VERSION_MAJOR $MAJOR/" "$VERSION_H_FILE"
sed -i "s/^#define INBE_VERSION_MINOR .*/#define INBE_VERSION_MINOR $MINOR/" "$VERSION_H_FILE"
sed -i "s/^#define INBE_VERSION_PATCH .*/#define INBE_VERSION_PATCH $PATCH/" "$VERSION_H_FILE"
sed -i "s/^#define INBE_VERSION_STRING .*/#define INBE_VERSION_STRING \"$LATEST_VERSION\"/" "$VERSION_H_FILE"

echo "✓ Updated $VERSION_H_FILE"

# Create changelog directory
mkdir -p "$CHANGELOG_DIR"

# Generate changelog files (position-based: oldest = 1, newest = N)
# Reverse versions so oldest is processed first
TOTAL_VERSIONS=$(echo "$VERSIONS" | wc -l)
POSITION=0
while IFS= read -r VERSION; do
    POSITION=$((POSITION + 1))
    # Calculate position from end (newest gets highest number)
    CODE=$((TOTAL_VERSIONS - POSITION + 1))
    OUTPUT_FILE="$CHANGELOG_DIR/$CODE.txt"

    # Skip if file already exists
    if [ -f "$OUTPUT_FILE" ]; then
        continue
    fi

    # Extract changelog content for this version
    CHANGELOG_CONTENT=$(awk -v ver="$VERSION" '
        BEGIN { in_section=0 }
        /^## \[/ {
            if (in_section) exit
            if (index($0, ver) > 0) {
                in_section=1
                next
            }
        }
        in_section {
            # Print section headers and bullet points
            if (/^### / || /^- /) print
        }
    ' "$CHANGELOG_FILE")

    echo "$CHANGELOG_CONTENT" > "$OUTPUT_FILE"
    echo "✓ Created $OUTPUT_FILE"
done <<< "$VERSIONS"

echo ""
echo "Done! Build and test the app before committing."
