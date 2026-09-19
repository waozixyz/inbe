# Inbe source ownership

Product behavior is maintained in `.kry`: application setup and rendering,
practice and habit rules, account/sync orchestration, database policy, audio
selection, desktop tray menus, screenshot scenes, and browser actions.
`app/app_types.kry` owns the application records and enums. C declarations are
emitted by k2c into the build directory.

The main application uses direct screen dispatch. Tray implementation lives in
`platform/tray.kry`, without a second set of forwarding functions. Native host
startup calls the same `.kry` application setup on each platform.

## UI composition

- Use `Text(TextProps)`, `Image(ImageProps)`, and `Button(ButtonProps)` directly.
- Supply text bounds, font, alignment, and wrapping in props. Bounded one-line
  text uses the standard widget's clipping instead of app-owned UTF-8 chopping.
- Put product presentation rules in `assets/styles/inbe.kss`. It augments the
  selected Kryon style pack and shares its color tokens.
- `app/widgets.kry` composes the product's panels and guide content from
  standard controls. Kryon's shared Modal, Guide, and Reorder policies own
  their geometry; Popup owns dismissal and capture for volume sliders.
- Session action rows use shared toolbar geometry and 44-unit buttons with gaps.
  Route choices, guide steps, and practice animations remain Inbe behavior.
- Background art and Sun Salutation atlas frames use asset-backed Image props.
  Kryon owns image loading, caching, cropping, and contain/cover fitting.

## Native boundary

The remaining C handles platform integration: window setup/frame submission,
Android JNI and insets, browser JavaScript interop, and Plan 9 system adapters.
`storage_json_builder.c` contains only the C varargs bridge; buffer growth,
escaping, failure state, and timestamp serialization live in `storage/json.kry`.
The checked JSON builder is retained because the current Kryon JSON emitter
does not expose the allocation-failure result required by Inbe's sync writer.

Database schemas, setting keys, sync protocol v6, tombstones, and recovery
policy are preserved. This migration does not introduce a data conversion.

## Building and checking

Run `make native test` with the platform's normal build configuration. On omega,
set `PKG_CONFIG_PATH=/home/wao/.local/sdl2/lib/pkgconfig`. The tests consume the
freshly generated app modules, including the font selector and UI metrics.
`make web` and the Android CMake build generate the same `.kry` sources for their
platforms. `make kry-c-plan9` produces Plan 9 C and its source list.

Kryon commit `c645a7016` removes the frontend's 16-constant-per-module limit.
The compiler regression covers a module with 80 constants; language tests cover
C, C++, and Go. Runtime/compiler changes belong upstream on Kryon master, then
come into Inbe through a clean submodule update.

Kryon commit `cde33e956` supplies the Canvas backend's
paint-layer transform capture and restore. Its rendering regression covers
rotation, zoom, and replay from a high-DPI window into a render texture. Web
builds enable Emscripten's zlib port for the shared archive implementation.
