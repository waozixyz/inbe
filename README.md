# Inner Breeze

Inner Breeze is a free, open-source app for breathing, meditation, and habit tracking.

## Build

Use the Nix development shell:

```bash
nix develop
make native
make run
```

Install locally:

```bash
make install
```

This installs the app to `~/.local/lib/inbe` and links `~/.local/bin/inbe`.
If `~/bin` exists, it links `~/bin/inbe` too.

## Other Targets

```bash
make test
make android-debug
make web
make appimage
make windows
```

## Android

Run Android build and emulator commands from the Nix development shell:

```bash
nix develop
```

Build a debug APK:

```bash
make android-debug-fast
```

Create and launch an Android 17/API 37.1 AVD for Pixel storage testing:

```bash
ANDROID_API=37.1 bash scripts/create-avd.sh inbe-android-37-1
AVD_NAME=inbe-android-37-1 bash scripts/emulator.sh
```

Android 17 system images are listed by Google as `android-37.0`,
`android-37.1`, and `android-CinnamonBun`. The repo AVD script supports the
16 KB page-size image types such as `google_apis_playstore_ps16k`.

Native Android storage must use the app's private internal data directory from
`ANativeActivity->internalDataPath`, not the process working directory. Inbe's
SQLite database lives under that app-private root and is removed when the app is
uninstalled.

## Project Layout

- `src/` - app code
- `assets/` - images, fonts, and sounds
- `locales/` - translations
- `droid/` - Android project
- `site/` - website
- `vendor/flint/` - Flint UI/runtime submodule

## Notes

Inbe stores app data locally in SQLite. Optional sync can mirror user-owned
data through a Lyra-compatible sync server.

## License

MIT. See [LICENSE](LICENSE).
