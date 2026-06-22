#!/usr/bin/env bash
set -e

# Icon Generation Script for Inner Breeze
# Generates transparent icons by default, plus maskable/adaptive variants where
# platforms clip icons into a shape and need a full-bleed background.

SKY_BLUE="#87CEEB"
SOURCE_IMAGE="${1:-flint/icons/inbe.png}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "🔨 Inner Breeze Icon Generator"
echo "=============================="
echo

# Validate ImageMagick installation
if ! command -v magick &> /dev/null; then
    echo "❌ Error: ImageMagick v7 not found. Please install ImageMagick:"
    echo "   Ubuntu/Debian: sudo apt-get install imagemagick"
    echo "   macOS: brew install imagemagick"
    echo "   Fedora: sudo dnf install imagemagick"
    exit 1
fi

# Validate source image
if [ ! -f "$SOURCE_IMAGE" ]; then
    echo "❌ Error: Source image not found: $SOURCE_IMAGE"
    exit 1
fi

echo "✅ Using source image: $SOURCE_IMAGE"
echo "🎨 Maskable/adaptive background color: $SKY_BLUE (Sky Blue)"
echo

# Create output directories
mkdir -p "$PROJECT_ROOT/droid/app/src/main/res/drawable"
mkdir -p "$PROJECT_ROOT/droid/app/src/main/res/mipmap-mdpi"
mkdir -p "$PROJECT_ROOT/droid/app/src/main/res/mipmap-hdpi"
mkdir -p "$PROJECT_ROOT/droid/app/src/main/res/mipmap-xhdpi"
mkdir -p "$PROJECT_ROOT/droid/app/src/main/res/mipmap-xxhdpi"
mkdir -p "$PROJECT_ROOT/droid/app/src/main/res/mipmap-xxxhdpi"
mkdir -p "$PROJECT_ROOT/droid/app/src/main/res/mipmap-anydpi-v26"
mkdir -p "$PROJECT_ROOT/windows"
mkdir -p "$PROJECT_ROOT/packaging/linux/appimage"
mkdir -p "$PROJECT_ROOT/packaging/chrome-web-store/icons"
mkdir -p "$PROJECT_ROOT/web-assets/icons"
mkdir -p "$PROJECT_ROOT/site-icons"
mkdir -p "$PROJECT_ROOT/fastlane/metadata/android/en-US/images"

generate_transparent() {
    local size=$1
    local output=$2
    magick "$SOURCE_IMAGE" \
        -filter lanczos \
        -resize "${size}x${size}" \
        -strip \
        +set date:create +set date:modify +set date:timestamp \
        -define png:exclude-chunk=time \
        "$output"
}

generate_maskable() {
    local size=$1
    local output=$2
    local logo_size=$((size * 4 / 5))
    magick "$SOURCE_IMAGE" \
        -filter lanczos \
        -resize "${logo_size}x${logo_size}" \
        -background none \
        -gravity center \
        -extent "${size}x${size}" \
        \( -size "${size}x${size}" "xc:$SKY_BLUE" \) +swap \
        -compose over \
        -composite \
        -strip \
        +set date:create +set date:modify +set date:timestamp \
        -define png:exclude-chunk=time \
        "$output"
}

write_android_background() {
    cat > "$PROJECT_ROOT/droid/app/src/main/res/drawable/ic_launcher_background.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<shape xmlns:android="http://schemas.android.com/apk/res/android"
    android:shape="rectangle">
    <solid android:color="$SKY_BLUE" />
</shape>
EOF
}

echo "📱 Generating Android icons..."

# Android legacy launcher icons support transparency.
generate_transparent 48 "$PROJECT_ROOT/droid/app/src/main/res/mipmap-mdpi/ic_launcher.png"
generate_transparent 72 "$PROJECT_ROOT/droid/app/src/main/res/mipmap-hdpi/ic_launcher.png"
generate_transparent 96 "$PROJECT_ROOT/droid/app/src/main/res/mipmap-xhdpi/ic_launcher.png"
generate_transparent 144 "$PROJECT_ROOT/droid/app/src/main/res/mipmap-xxhdpi/ic_launcher.png"
generate_transparent 192 "$PROJECT_ROOT/droid/app/src/main/res/mipmap-xxxhdpi/ic_launcher.png"

generate_transparent 48 "$PROJECT_ROOT/droid/app/src/main/res/mipmap-mdpi/ic_launcher_round.png"
generate_transparent 72 "$PROJECT_ROOT/droid/app/src/main/res/mipmap-hdpi/ic_launcher_round.png"
generate_transparent 96 "$PROJECT_ROOT/droid/app/src/main/res/mipmap-xhdpi/ic_launcher_round.png"
generate_transparent 144 "$PROJECT_ROOT/droid/app/src/main/res/mipmap-xxhdpi/ic_launcher_round.png"
generate_transparent 192 "$PROJECT_ROOT/droid/app/src/main/res/mipmap-xxxhdpi/ic_launcher_round.png"

# Android adaptive icons use a full-bleed background plus a transparent foreground.
write_android_background
generate_transparent 432 "$PROJECT_ROOT/droid/app/src/main/res/drawable/ic_launcher_foreground.png"

echo "✅ Android icons generated"

echo "🪟 Generating Windows icon..."

# ICO supports transparency; do not flatten.
magick \
    "$SOURCE_IMAGE" -resize 256x256 -strip \
    "$SOURCE_IMAGE" -resize 192x192 -strip \
    "$SOURCE_IMAGE" -resize 32x32 -strip \
    "$PROJECT_ROOT/windows/inbe.ico"

echo "✅ Windows icon generated"

echo "🐧 Generating Linux AppImage icon..."

# AppImage desktop icons are PNGs and support transparency.
generate_transparent 256 "$PROJECT_ROOT/packaging/linux/appimage/inbe.png"

echo "✅ Linux AppImage icon generated"

echo "🏪 Generating Fastlane store icon..."

# F-Droid/Fastlane store listing icon expects a 512x512 PNG.
generate_maskable 512 "$PROJECT_ROOT/fastlane/metadata/android/en-US/images/icon.png"

echo "✅ Fastlane store icon generated"

echo "🌐 Generating Chrome Web Store icons..."

# Chrome Web Store icons (transparent, various sizes)
generate_transparent 16 "$PROJECT_ROOT/packaging/chrome-web-store/icons/icon-16.png"
generate_transparent 32 "$PROJECT_ROOT/packaging/chrome-web-store/icons/icon-32.png"
generate_transparent 48 "$PROJECT_ROOT/packaging/chrome-web-store/icons/icon-48.png"
generate_transparent 128 "$PROJECT_ROOT/packaging/chrome-web-store/icons/icon-128.png"

echo "✅ Chrome Web Store icons generated"

echo "🌐 Generating Web/PWA icons..."

# Web/PWA icons (for manifest.json and web_shell.html)
generate_transparent 192 "$PROJECT_ROOT/site-icons/icon-192x192.png"
generate_transparent 512 "$PROJECT_ROOT/site-icons/icon-512x512.png"
generate_maskable 192 "$PROJECT_ROOT/site-icons/maskable-192x192.png"
generate_maskable 512 "$PROJECT_ROOT/site-icons/maskable-512x512.png"
generate_transparent 180 "$PROJECT_ROOT/site-icons/apple-touch-icon.png"

# Favicon PNG supports transparency.
generate_transparent 32 "$PROJECT_ROOT/site-icons/favicon-32x32.png"

echo "✅ Web/PWA icons generated"

echo
echo "🎉 Icon generation complete!"
echo
echo "Generated icons:"
echo "  📱 Android: mipmap PNGs + adaptive drawable foreground/background"
echo "  🪟 Windows: windows/inbe.ico (256, 192, 32 resolutions)"
echo "  🐧 Linux:   packaging/linux/appimage/inbe.png (256x256 transparent)"
echo "  🏪 Store:   fastlane/metadata/android/en-US/images/icon.png (512x512)"
echo "  🌐 Chrome:  packaging/chrome-web-store/icons/ (16, 32, 48, 128)"
echo "  🌐 Web:    site-icons/ (transparent + maskable PWA icons)"
echo
echo "💡 Tip: Review generated icons before committing to version control."
