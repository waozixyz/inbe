# Inbe source ownership

## Remaining migration work (2026-09-20)

This section records the current working tree, not a shipped or verified
state. The latest Kryon and Inbe `.kry` moves are uncommitted; code generation,
builds, tests, screenshots, and Android/Plan 9 checks have not run for them.
Kryon's source-level and Button evidence checklist is in
[the upstream UI migration plan](../../kryon/plan/UI_MIGRATION_REMAINING.md).

| Area | Current state | What closes it |
|---|---|---|
| App UI contract | `src/app/app.h` still has handwritten declarations for screen, widget, route, audio, and presentation functions implemented in `.kry`. `app_fwd.h`, `app_internal.h`, `app_nav.h`, screen/practice headers, and platform headers also need declaration-by-declaration review. | Move UI records and callable contracts into `.kry`, consume k2c-generated headers, and delete redundant handwritten UI declarations without introducing compatibility headers. Keep genuine OS/FFI contracts only. Build all maintained callers after removal. |
| Native C boundary | `src/platform/main_host.c`, `src/app/app_web_bridge.c`, `src/platform/android/{android_device.c,android_insets.c,android_share.c}`, Plan 9 entry/SQLite/import adapters, and `src/storage/storage_json_builder.c` remain. | Audit every branch for UI, lifecycle, theme, and data policy. Move any remaining product decision to `.kry`; document the OS/JNI/JS/SQLite/C-varargs effect left in C. Preserve the checked JSON builder failure contract. |
| Platform UI moves | Screenshot scenes/capture, startup and icon selection, Android lifecycle/share title, Plan 9 app loop and music fallback, and style tokens now have `.kry` owners. | Generate and build the changed sources on each supported path; check screenshot parity, icon installation, Android pause/resume/share, Plan 9 startup/music fallback, and style persistence. The Plan 9 `entry.c` must remain a process adapter only. |
| KSS ownership | `assets/styles/inbe.kss` supplies product presentation and `src/app/app_style.kry` supplies style behavior, but the current generated boundary and all live theme callers have not been reverified. | Review each hardcoded presentation choice and theme getter use, migrate reusable style choices upstream where necessary, then verify light/dark style switching and no-style fallback without changing the accepted appearance. |
| UI test ownership | UI shell checks exist; `tests/app_bottom_nav_test.c`, `frame_pacing_test.c`, `font_locale_test.c`, `screenshot_scene_test.sh`, and other C/UI fixtures still need ownership review. | Move UI behavior/assertions into `.kry` tests where the language can express them, leaving shell/C only for narrow host setup or process checks. Ensure the test graph compiles the generated modules; record exact results. |
| Background notification policy | Desktop tray, Chrome worker, and Android indicator/session code have working-tree edits for the rule that no background activity continues without a visible indicator. | Verify close/minimize/reopen, indicator loss, permission denial, Android foreground service and Chrome worker behavior on their actual targets. Confirm no timer or notification survives an absent indicator; record platform evidence. |
| Vendor and delivery | `vendor/kryon` has a pointer change but its worktree is clean; upstream Kryon has uncommitted UI changes. | Verify and commit Kryon on upstream `master` first, then move the clean Inbe submodule pointer to that exact commit. Build/verify Inbe and commit app changes on `master`. No vendor source edits. |

Verification requires separate user approval for each test and each Bend law.
After approval, Inbe's `make build-laws` is mandatory for native, web, and
Windows builds; `make clean-text-api-check button-api-check` covers the UI API
guards. Native checks need omega's SDL pkg-config path, and display/input
checks must run on a private Xvfb/Xephyr display with inherited display
variables scrubbed. Record Android, Plan 9, web, and Windows separately;
unavailable toolchains or devices stay unverified.

Most product behavior is maintained in `.kry`: application setup and rendering,
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

The frame loop, startup choices, screenshot scenes, native Android session
state transitions and debug practice decisions, and Plan 9 window loop are
authored in `.kry`.
The remaining C handles OS window calls, Android JNI conversion and synchronized
event handoff, browser JavaScript interop, and Plan 9 process entry and system
adapters. Plan 9's music playback, status, and unavailable control behavior are authored in
`platform/plan9/meditation_music.kry`; the Plan 9 C music stub has been removed.
The desktop icon asset is selected in `platform/app_host.kry`; the native host
only decodes and installs it.
These latest moves still require native, Android, and Plan 9 verification.
Android Java still owns notification permission, indicator observation, and
foreground-service effects. The session pause/resume and background-timer
decision is in `platform/android/android_lifecycle.kry`.
Android share-sheet titles are selected by `.kry` callers; the JNI adapter
receives the title with the data it shares.
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
