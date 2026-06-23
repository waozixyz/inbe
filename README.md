# Inner Breeze

Inner Breeze is a free, open-source practice app for breathing, meditation, and habit tracking.

## Features

- Guided practice sessions with visual and audio cues
- Mind, Yoga, and Fitness organization for habits and practice views
- Configurable breathing sessions and meditation timers
- Habit tracking with session history
- Local SQLite storage with import/export support
- Theme customization with light and dark variants
- Touch-optimized UI for desktop and mobile

## Building

inbe uses [raylib](https://github.com/raysan5/raylib) and requires a Nix development environment.

### Prerequisites

- [Nix](https://nixos.org/download.html) with flakes enabled
- For Android builds: Java 17+ JDK

### Native Build

```bash
# Enter the development shell
nix develop

# Build for your current platform
make native

# Run
make run
```

### Cross-Platform Builds

```bash
# Android debug APK
make android-debug

# Android release APK
make android-release PASSWORD=your-keystore-password
```

### Android Emulator Testing

For automated testing with camera notch (cutout) support:

```bash
# Enter the development shell and run everything automatically
nix develop
./scripts/emulator.sh    # Creates and launches the test AVD
make android-install     # Builds, installs, and launches the app
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

- `src/` - Inbe application code
- `vendor/` - external third-party dependencies
- `vendor/flint/` - first-party UI/runtime support code used directly by Inbe
- `wasm4/` - Standalone WASM-4 version
- `assets/` - Images and sounds
- `droid/` - Android build configuration

## License

MIT - see [LICENSE](LICENSE)
