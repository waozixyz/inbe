#!/usr/bin/env bash
# update_version.sh - Update version from CHANGELOG.md
# Usage: ./update_version.sh

set -e

CHANGELOG_FILE="CHANGELOG.md"
GRADLE_FILE="droid/app/build.gradle"

# Check if files exist
if [ ! -f "$CHANGELOG_FILE" ]; then
    echo "Error: $CHANGELOG_FILE not found"
    exit 1
fi

if [ ! -f "$GRADLE_FILE" ]; then
    echo "Error: $GRADLE_FILE not found"
    exit 1
fi

# Extract latest version from CHANGELOG (first ## [X.Y.Z] pattern)
VERSION=$(grep -m1 '^## \[' "$CHANGELOG_FILE" | sed 's/^## \[\([^]]*\)\].*/\1/')

if [ -z "$VERSION" ]; then
    echo "Error: No version found in $CHANGELOG_FILE"
    echo "Expected format: ## [1.0.0] - YYYY-MM-DD"
    exit 1
fi

# Validate version format (basic semver check)
if ! [[ "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "Error: Invalid version format: $VERSION"
    echo "Expected semantic versioning format: X.Y.Z"
    exit 1
fi

# Get current versionCode and increment
CURRENT_CODE=$(grep "versionCode" "$GRADLE_FILE" | awk '{print $2}')
NEW_CODE=$((CURRENT_CODE + 1))

# Get current versionName for comparison
CURRENT_NAME=$(grep "versionName" "$GRADLE_FILE" | sed 's/.*"\([^"]*\)".*/\1/')

echo "Current: versionName = $CURRENT_NAME, versionCode = $CURRENT_CODE"
echo "New:     versionName = $VERSION, versionCode = $NEW_CODE"

# Confirm before making changes
read -p "Apply changes? (y/N) " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Aborted"
    exit 0
fi

# Backup original file
cp "$GRADLE_FILE" "$GRADLE_FILE.bak"

# Update gradle file
sed -i "s/versionCode $CURRENT_CODE/versionCode $NEW_CODE/" "$GRADLE_FILE"
sed -i "s/versionName \"$CURRENT_NAME\"/versionName \"$VERSION\"/" "$GRADLE_FILE"

echo "✓ Updated $GRADLE_FILE"
echo ""
echo "Changes:"
diff -u "$GRADLE_FILE.bak" "$GRADLE_FILE" || true
rm "$GRADLE_FILE.bak"

echo ""
echo "Done! Build and test the app before committing."
