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
# versionCode = patch + 1 (since 1.0.0 = code 1)
VERSION_CODE=$(($(echo "$LATEST_VERSION" | cut -d. -f3) + 1))

# Get current versionName
CURRENT_NAME=$(grep "versionName" "$GRADLE_FILE" | sed 's/.*"\([^"]*\)".*/\1/')
CURRENT_CODE=$(grep "versionCode" "$GRADLE_FILE" | awk '{print $2}')

echo "Updating to: $LATEST_VERSION (code $VERSION_CODE)"
echo "Current: $CURRENT_NAME (code $CURRENT_CODE)"

# Update gradle file with latest version
sed -i "s/versionCode $CURRENT_CODE/versionCode $VERSION_CODE/" "$GRADLE_FILE"
sed -i "s/versionName \"$CURRENT_NAME\"/versionName \"$LATEST_VERSION\"/" "$GRADLE_FILE"

echo "✓ Updated $GRADLE_FILE"

# Create changelog directory
mkdir -p "$CHANGELOG_DIR"

# Generate only missing changelog files
while IFS= read -r VERSION; do
    # versionCode = patch + 1
    CODE=$(($(echo "$VERSION" | cut -d. -f3) + 1))
    OUTPUT_FILE="$CHANGELOG_DIR/$CODE.txt"

    # Skip if file already exists
    if [ -f "$OUTPUT_FILE" ]; then
        continue
    fi

    # Extract changelog content for this version
    CHANGELOG_CONTENT=$(awk -v version="[$VERSION]" '
        /^## \[/ {
            if (found) exit
            if ($0 ~ "\\[\\[" version "\\]\\]") {
                found=1
                next
            }
        }
        found && /^### / { subsection=1 }
        found { print }
    ' "$CHANGELOG_FILE" | sed '1d;/^$/d' | head -n -1)

    echo "$CHANGELOG_CONTENT" > "$OUTPUT_FILE"
    echo "✓ Created $OUTPUT_FILE"
done <<< "$VERSIONS"

echo ""
echo "Done! Build and test the app before committing."
