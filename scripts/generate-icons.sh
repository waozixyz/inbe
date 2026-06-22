#!/bin/bash
set -e

# Icon Generation Script for Inner Breeze
# Generates icons for all platforms using ImageMagick with pixel-perfect scaling

SKY_BLUE="#87CEEB"
SOURCE_IMAGE="${1:-flint/icons/inbe.png}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

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
echo "🎨 Background color: $SKY_BLUE (Sky Blue)"
echo

# Create output directories
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

# Function to generate icon with background
generate_with_background() {
    local size=$1
    local output=$2
    magick "$SOURCE_IMAGE" \
        -filter lanczos \
        -resize "${size}x${size}" \
        -background "$SKY_BLUE" \
        -compose over \
        -flatten \
        "$output"
}

# Function to generate icon with transparency
generate_transparent() {
    local size=$1
    local output=$2
    magick "$SOURCE_IMAGE" \
        -filter lanczos \
        -resize "${size}x${size}" \
        "$output"
}

echo "📱 Generating Android icons..."

# Android standard icons (with background)
generate_with_background 48 "$PROJECT_ROOT/droid/app/src/main/res/mipmap-mdpi/ic_launcher.png"
generate_with_background 72 "$PROJECT_ROOT/droid/app/src/main/res/mipmap-hdpi/ic_launcher.png"
generate_with_background 96 "$PROJECT_ROOT/droid/app/src/main/res/mipmap-xhdpi/ic_launcher.png"
generate_with_background 144 "$PROJECT_ROOT/droid/app/src/main/res/mipmap-xxhdpi/ic_launcher.png"
generate_with_background 192 "$PROJECT_ROOT/droid/app/src/main/res/mipmap-xxxhdpi/ic_launcher.png"

# Android round icons (with background)
generate_with_background 48 "$PROJECT_ROOT/droid/app/src/main/res/mipmap-mdpi/ic_launcher_round.png"
generate_with_background 72 "$PROJECT_ROOT/droid/app/src/main/res/mipmap-hdpi/ic_launcher_round.png"
generate_with_background 96 "$PROJECT_ROOT/droid/app/src/main/res/mipmap-xhdpi/ic_launcher_round.png"
generate_with_background 144 "$PROJECT_ROOT/droid/app/src/main/res/mipmap-xxhdpi/ic_launcher_round.png"
generate_with_background 192 "$PROJECT_ROOT/droid/app/src/main/res/mipmap-xxxhdpi/ic_launcher_round.png"

# Android adaptive icon foregrounds (transparent, 108x108 for 512x512 container)
generate_transparent 108 "$PROJECT_ROOT/droid/app/src/main/res/mipmap-anydpi-v26/ic_launcher_foreground.png"
generate_transparent 108 "$PROJECT_ROOT/droid/app/src/main/res/mipmap-anydpi-v26/ic_launcher_round_foreground.png"

echo "✅ Android icons generated"

echo "🪟 Generating Windows icon..."

# Generate multi-resolution ICO for Windows
magick \
    "$SOURCE_IMAGE" -resize 256x256 -background "$SKY_BLUE" -flatten \
    "$SOURCE_IMAGE" -resize 192x192 -background "$SKY_BLUE" -flatten \
    "$SOURCE_IMAGE" -resize 32x32 -background "$SKY_BLUE" -flatten \
    "$PROJECT_ROOT/windows/inbe.ico"

echo "✅ Windows icon generated"

echo "🐧 Generating Linux AppImage icon..."

# Linux AppImage icon (scalable PNG)
generate_with_background 256 "$PROJECT_ROOT/packaging/linux/appimage/inbe.png"

echo "✅ Linux AppImage icon generated"

echo "🌐 Generating Chrome Web Store icons..."

# Chrome Web Store icons (transparent, various sizes)
generate_transparent 16 "$PROJECT_ROOT/packaging/chrome-web-store/icons/icon-16.png"
generate_transparent 32 "$PROJECT_ROOT/packaging/chrome-web-store/icons/icon-32.png"
generate_transparent 48 "$PROJECT_ROOT/packaging/chrome-web-store/icons/icon-48.png"
generate_transparent 128 "$PROJECT_ROOT/packaging/chrome-web-store/icons/icon-128.png"

echo "✅ Chrome Web Store icons generated"

echo "🌐 Generating Web/PWA icons..."

# Web/PWA icons (for manifest.json and web_shell.html)
generate_transparent 192 "$PROJECT_ROOT/web-assets/icons/icon-192x192.png"
generate_transparent 512 "$PROJECT_ROOT/web-assets/icons/icon-512x512.png"
generate_transparent 180 "$PROJECT_ROOT/web-assets/icons/apple-touch-icon.png"

# Favicon for web_shell.html
generate_with_background 32 "$PROJECT_ROOT/web-assets/icons/favicon-32x32.png"

echo "✅ Web/PWA icons generated"

echo
echo "🎉 Icon generation complete!"
echo
echo "Generated icons:"
echo "  📱 Android: 6 mipmap directories (mdpi → xxxhdpi + anydpi-v26)"
echo "  🪟 Windows: windows/inbe.ico (256, 192, 32 resolutions)"
echo "  🐧 Linux:   packaging/linux/appimage/inbe.png (256x256)"
echo "  🌐 Chrome:  packaging/chrome-web-store/icons/ (16, 32, 48, 128)"
echo "  🌐 Web:    web-assets/icons/ (192, 512, apple-touch-icon, favicon-32x32)"
echo
echo "💡 Tip: Review generated icons before committing to version control."
