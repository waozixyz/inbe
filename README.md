# Inner Breeze

Inner Breeze - A free, open-source guided breathing meditation app based on the Wim Hof method.

## Features

- Guided breathing sessions with visual and audio cues
- Configurable breath counts and hold times
- Session history with local storage
- 8 color themes (Forest, Ocean, Sky, Sunset, Lavender - each with light/dark variants)
- Sound effects (breath in/out, bell)
- Fullscreen mode with proper DPI scaling
- Touch-optimized UI for mobile devices

## Building

inbe uses [raylib](https://github.com/raysan5/raylib) and requires a Nix development environment.

### Prerequisites

- [Nix](https://nixos.org/download.html) with flakes enabled
- For Android builds: Java 17+ JDK

### Native Build

```bash
# Enter the development shell
nix develop

# Build for your current platform (Linux x86_64/aarch64)
make

# Run
./build/linux/inbe-linux-$(uname -m)
```

### Cross-Platform Builds

```bash
# Linux (both architectures)
make linux

# Windows (from Linux)
make windows

# Android APK
make android

# WebAssembly
make web
```

### Android Emulator Testing

For automated testing with camera notch (cutout) support:

```bash
# Enter the development shell and run everything automatically
nix develop
./scripts/emulator.sh    # Creates AVD, launches emulator, builds & runs app
```

This single script handles:
- AVD creation (if missing)
- Emulator launch
- APK build
- App installation and launch

The AVD is configured with:
- **Device**: Pixel 6 (punch-hole camera style)
- **Display**: 1080×2400 @ 420dpi
- **RAM**: 4GB with hardware acceleration
- **Cutout**: Punch-hole camera for testing window insets

### Platform-Specific Notes

- **Linux**: Requires SDL2 runtime libraries
- **Windows**: Standalone executable, no dependencies
- **Android**: Requires Android NDK in the development shell
- **Web**: Requires Emscripten toolchain

## Project Structure

- `src/` - Main application code (raylib frontend)
- `libinbe/` - Core meditation logic (platform-agnostic)
- `assets/` - Images and sounds
- `themes/` - Color theme definitions
- `droid/` - Android build configuration
- `vendor/raylib/` - raylib submodule

## License

MIT - see [LICENSE](LICENSE)
