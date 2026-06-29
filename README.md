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
