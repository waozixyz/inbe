<p align="center">
  <img src="assets/app/readme-banner.png" alt="Inner Breeze" width="100%">
</p>

<p align="center">
  <a href="https://play.google.com/store/apps/details?id=xyz.waozi.inbe">
    <img src="assets/app/badge-google-play.png" alt="Get it on Google Play" height="56">
  </a>
  <a href="https://f-droid.org/packages/xyz.waozi.inbe/">
    <img src="assets/app/badge-f-droid.png" alt="Get it on F-Droid" height="56">
  </a>
  <a href="https://waozi.itch.io/inbe">
    <img src="https://static.itch.io/images/badge.svg" alt="Download on itch.io" height="56">
  </a>
</p>

# Inner Breeze

Inner Breeze is a free, open-source practice app for breathing, meditation, and
habit tracking. It works offline, stores data locally in SQLite, and can
optionally sync user-owned data through a Ksync-compatible sync server.

## Features

- Guided practice sessions with visual and audio cues
- Mind, Yoga, and Fitness organization for different practice routines
- Customizable breathing sessions, meditation timers, and habit counters
- Habit tracking with session history and linked practice counts
- Local import and export support
- Theme customization with light and dark variants

## Build

Initialize the submodules once:

```bash
git submodule update --init --recursive
```

Build and run the desktop app:

```bash
make native
make run
```

Run the terminal backend:

```bash
make tui
```

Build Android artifacts:

```bash
make android-debug
PASSWORD=your-keystore-password make android-release
PASSWORD=your-keystore-password make android-bundle
```

Build the web app:

```bash
make web
make site
```

Run the test suite:

```bash
make test
```

Native binaries are written to `build/bin/<platform>/`. Release artifacts are
written under `build/dist/`.

To debug Inbe against the root Kryon checkout instead of the vendored
submodule, override `KRYON_DIR`:

```bash
make run KRYON_DIR=../kryon
```

Use this only for local debugging. Permanent Kryon fixes should be committed in
the root Kryon repository and then brought into Inbe by updating
`vendor/kryon`.

## Project Layout

- `src/` - app code
- `assets/` - images, fonts, and sounds
- `locales/` - translations
- `droid/` - Android project
- `site/` - website
- `vendor/kryon/` - Kryon UI/runtime submodule

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

## License

BSD 3-Clause. See [LICENSE](LICENSE).
