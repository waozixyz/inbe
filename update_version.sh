#!/usr/bin/env bash
# update_version.sh - Update version from CHANGELOG.md (master truth)
# Usage: ./update_version.sh

set -e

CHANGELOG_FILE="CHANGELOG.md"
GRADLE_FILE="droid/app/build.gradle"
WINDOWS_RC_FILE="windows/inbe.rc"
CHANGELOG_DIR="fastlane/metadata/android/en-US/changelogs"
CLICK_MANIFEST_FILE="packaging/click/manifest.json"
CLICK_CONTROL_FILE="packaging/click/control"
CLICK_METAINFO_FILE="packaging/click/inbe.metainfo.xml"
FIREFOX_ADDONS_MANIFEST_FILE="packaging/firefox-addons/manifest.json"

replace_in_file() {
    local expr="$1"
    local file="$2"
    local tmp

    tmp=$(mktemp "${file}.XXXXXX")
    sed "$expr" "$file" > "$tmp"
    mv "$tmp" "$file"
}

# Extract all versions from CHANGELOG
VERSIONS=$(grep '^## \[[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\]' "$CHANGELOG_FILE" | sed 's/^## \[\([^]]*\)\].*/\1/')

if [ -z "$VERSIONS" ]; then
    echo "Error: No versions found in $CHANGELOG_FILE"
    echo "Expected format: ## [1.0.0] - YYYY-MM-DD"
    exit 1
fi

# Get latest version for gradle update
LATEST_VERSION=$(echo "$VERSIONS" | head -n 1)

# Get current gradle values
CURRENT_NAME=$(grep "versionName" "$GRADLE_FILE" | sed 's/.*"\([^"]*\)".*/\1/')
CURRENT_CODE=$(grep "versionCode" "$GRADLE_FILE" | awk '{print $2}')

# Android versionCode: just increment by 1 for new versions
# If version hasn't changed, keep the same code
if [ "$LATEST_VERSION" = "$CURRENT_NAME" ]; then
    VERSION_CODE=$CURRENT_CODE
else
    VERSION_CODE=$((CURRENT_CODE + 1))
fi

echo "Updating to: $LATEST_VERSION (code $VERSION_CODE)"
echo "Current: $CURRENT_NAME (code $CURRENT_CODE)"

# Update gradle file with latest version
replace_in_file "s/versionCode $CURRENT_CODE/versionCode $VERSION_CODE/" "$GRADLE_FILE"
replace_in_file "s/versionName \"$CURRENT_NAME\"/versionName \"$LATEST_VERSION\"/" "$GRADLE_FILE"

echo "✓ Updated $GRADLE_FILE"

# Update version.h
VERSION_H_FILE="src/core/version.h"
MAJOR=$(echo "$LATEST_VERSION" | cut -d. -f1)
MINOR=$(echo "$LATEST_VERSION" | cut -d. -f2)
PATCH=$(echo "$LATEST_VERSION" | cut -d. -f3)

if [ ! -f "$VERSION_H_FILE" ]; then
    echo "Error: $VERSION_H_FILE not found"
    exit 1
fi

replace_in_file "s/^#define INBE_VERSION_MAJOR .*/#define INBE_VERSION_MAJOR $MAJOR/" "$VERSION_H_FILE"
replace_in_file "s/^#define INBE_VERSION_MINOR .*/#define INBE_VERSION_MINOR $MINOR/" "$VERSION_H_FILE"
replace_in_file "s/^#define INBE_VERSION_PATCH .*/#define INBE_VERSION_PATCH $PATCH/" "$VERSION_H_FILE"
replace_in_file "s/^#define INBE_VERSION_STRING .*/#define INBE_VERSION_STRING \"$LATEST_VERSION\"/" "$VERSION_H_FILE"

echo "✓ Updated $VERSION_H_FILE"

if [ -f "$WINDOWS_RC_FILE" ]; then
    WINDOWS_VERSION="$MAJOR,$MINOR,$PATCH,0"
    replace_in_file "s/^ FILEVERSION .*/ FILEVERSION $WINDOWS_VERSION/" "$WINDOWS_RC_FILE"
    replace_in_file "s/^ PRODUCTVERSION .*/ PRODUCTVERSION $WINDOWS_VERSION/" "$WINDOWS_RC_FILE"
    replace_in_file "s/^\([[:space:]]*VALUE \"FileVersion\", \).*/\1\"$LATEST_VERSION\"/" "$WINDOWS_RC_FILE"
    replace_in_file "s/^\([[:space:]]*VALUE \"ProductVersion\", \).*/\1\"$LATEST_VERSION\"/" "$WINDOWS_RC_FILE"
    echo "✓ Updated $WINDOWS_RC_FILE"
else
    echo "Warning: $WINDOWS_RC_FILE not found"
fi

if [ -f "$CLICK_MANIFEST_FILE" ]; then
    replace_in_file "s/^\([[:space:]]*\"version\": \)\"[^\"]*\"/\1\"$LATEST_VERSION\"/" "$CLICK_MANIFEST_FILE"
    echo "✓ Updated $CLICK_MANIFEST_FILE"
else
    echo "Warning: $CLICK_MANIFEST_FILE not found"
fi

if [ -f "$CLICK_CONTROL_FILE" ]; then
    replace_in_file "s/^Version: .*/Version: $LATEST_VERSION/" "$CLICK_CONTROL_FILE"
    echo "✓ Updated $CLICK_CONTROL_FILE"
else
    echo "Warning: $CLICK_CONTROL_FILE not found"
fi

if [ -f "$CLICK_METAINFO_FILE" ]; then
    replace_in_file "s/<release version=\"[^\"]*\" date=\"[^\"]*\"/<release version=\"$LATEST_VERSION\" date=\"$(date +%F)\"/" "$CLICK_METAINFO_FILE"
    echo "✓ Updated $CLICK_METAINFO_FILE"
else
    echo "Warning: $CLICK_METAINFO_FILE not found"
fi

if [ -f "$FIREFOX_ADDONS_MANIFEST_FILE" ]; then
    replace_in_file "s/^\([[:space:]]*\"version\": \)\"[^\"]*\"/\1\"$LATEST_VERSION\"/" "$FIREFOX_ADDONS_MANIFEST_FILE"
    echo "✓ Updated $FIREFOX_ADDONS_MANIFEST_FILE"
else
    echo "Warning: $FIREFOX_ADDONS_MANIFEST_FILE not found"
fi

# Create changelog directory
mkdir -p "$CHANGELOG_DIR"

# Generate the Fastlane changelog for the Android versionCode being built.
# Fastlane file names are versionCode values, not changelog positions.
OUTPUT_FILE="$CHANGELOG_DIR/$VERSION_CODE.txt"
CHANGELOG_CONTENT=$(awk -v ver="$LATEST_VERSION" '
    BEGIN { in_section=0 }
    /^## \[/ {
        if (in_section) exit
        if (index($0, ver) > 0) {
            in_section=1
            next
        }
    }
    in_section {
        if (/^### /) {
            sub(/^### /, "")
            print
            print "---"
        } else if (/^- /) {
            print
        }
    }
' "$CHANGELOG_FILE")

echo "$CHANGELOG_CONTENT" > "$OUTPUT_FILE"
echo "✓ Updated $OUTPUT_FILE"

echo ""
echo "Done! Build and test the app before committing."
