# Inner Breeze

Inner Breeze is a free, open-source app for breathing, meditation, and habit tracking.

## Support

Monero:

```text
48ms5LfFrPJ2LUvqP9Mm5BhDSnZnqu14jB8XpAukw3jDBKxRAxYvq3k4fEwXY7kCY3LrtycMUayJZR1YJuyvJHCDCcyw6pA
```

Bitcoin:

```text
bc1qxzcetg50f6epgddc09n82xqn3zswlmk44235y5
```

Lightning:

```text
waozi@cake.cash
```

## Build

Build with the system toolchain and project Makefile:

```bash
make native
make run
```

On FreeBSD, use GNU Make:

```bash
gmake native
gmake run
```

To debug Inbe against the root Flint checkout instead of the vendored
submodule, override `FLINT_DIR`:

```bash
make run FLINT_DIR=../flint
```

On FreeBSD:

```bash
gmake run FLINT_DIR=../flint
```

Use this only for local debugging; permanent Flint fixes should be committed in
the root Flint repository and then brought into Inbe by updating
`vendor/flint`.

Native binaries are written to `build/bin/<platform>/`, for example
`build/bin/freebsd/inbe-freebsd-x86_64` on FreeBSD.

Install into the system prefix:

```bash
make install
```

By default this installs the binary, desktop entry, icon, and AppStream
metadata under `/usr/local`. Use `DESTDIR` for staged/package installs, or
`PREFIX` to choose another install root.

Install for only the current user:

```bash
make install-user
```

This installs under `~/.local`, including the desktop entry and icon.

## FreeBSD Package

Build a native FreeBSD package from inside the repo:

```bash
gmake package-freebsd
```

The package is written to `build/dist/freebsd/`, for example:

```bash
build/dist/freebsd/inbe-1.8.3-freebsd-x86_64.pkg
```

Install it with FreeBSD `pkg`:

```bash
sudo pkg install -y ./build/dist/freebsd/inbe-1.8.3-freebsd-x86_64.pkg
```

The package installs the `inbe` command plus desktop integration for menus such
as Xfce: `Inner Breeze` appears from the `.desktop` file, uses the bundled icon,
and includes AppStream metadata.

## Other Targets

General:

```bash
make test
make dist
make clean
```

Native desktop:

```bash
make native
make run
make install-user
make uninstall
```

FreeBSD:

```bash
gmake package-freebsd
```

Android:

```bash
make android-debug
make android-release
make android-bundle
```

Web:

```bash
make web
make site
make chrome-web-store
make firefox-addons
```

Linux:

```bash
make appimage
```

Ubuntu Touch:

```bash
make click
```

Windows:

```bash
make windows
```

Common outputs:

- `make native` builds `build/bin/<platform>/inbe-<platform>-<arch>`.
- `make appimage` builds a Linux AppImage under `build/dist/linux/`.
- `make web` builds and smoke-tests the web app, then writes `build/dist/inbe-web.zip`.
- `make windows` builds 64-bit and 32-bit Windows binaries and writes a zip under `build/dist/windows/`.
- `make click` builds the Ubuntu Touch Click package under `build/dist/click/`.
- `make dist` builds release artifacts; Android release targets require `PASSWORD`.

## Android

Build a debug APK:

```bash
make android-debug
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

BSD 3-Clause. See [LICENSE](LICENSE).
