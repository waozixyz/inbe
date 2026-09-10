# Inbe Agent Instructions

- Never edit files under `vendor/` from this repository.
- Kryon changes must be made in the core Kryon repository, committed there, pushed there, and then brought into Inbe by updating the Kryon submodule pointer.
- Do not add tests, policy checks, wrappers, compatibility aliases, or generated changes inside `vendor/` from Inbe.
- If a task appears to require a vendor change, stop and switch to the upstream project workflow first.
- When adding or changing localized strings, translate the content for every locale file being touched. Do not copy English placeholder text into non-English locales.
- Run `make no-vendor-edits` or `make test` before handing work back when a task touches Kryon or other vendored code.

## Clean Sync API Rule

- New Inbe releases use sync protocol v6 and the clean Kryon sync API. Do not
  add `Ksync`-prefixed types, functions, files, build variables, or compatibility
  wrappers. Legacy wire strings may remain only where migration code must read
  data produced by an already shipped release.
- Migration must be automatic, resumable, and non-destructive. Do not purge
  synced tombstones or legacy projections while a supported older installation
  can still return.

## Canonical Text Rule

- `Text` has exactly one supported form: `Text((TextProps){...})`. Put bounds,
  wrapping, clipping, color, alignment, and disabled state in `TextProps`.
- Do not add positional `Text` calls, parallel helpers such as `TextWrapped` or
  `TextColored`, or local wrappers that conceal the old signature. Change the
  maintained `.kry` or C source, never generated output or vendored Kryon code.
- Keep `make clean-text-api-check` passing. A UI migration is incomplete until
  the maintained source and freshly generated output build with upstream Kryon.

## Session Animation and Controls

- Desktop focus loss must never pause a practice, freeze its circle, suppress
  normal session updates, or switch it into the mobile background-timer path.
  Explicit user pause is separate from window focus. Preserve Android/web
  lifecycle handling without applying it to an unfocused desktop window.
- Keep Kryon's default hover, press, and focus transitions enabled. Do not
  disable transition cues globally in the app frame loop.
- Session audio controls use 44-unit targets (24-unit icons plus 10-unit
  padding on each side), scaled through Kryon, with space between controls.

## Readability Rule

- Buttons use Kryon's `Button(ButtonProps)` directly, including icon buttons
  and clickable cards. Do not restore app-local button wrappers or separately
  painted hover/click surfaces. Keep custom card content and domain gestures,
  but let the shared button own its surface and activation. Checkboxes and
  toggles use their own standard widgets. Run `make button-api-check`.
- The Lists screen starts with list tabs; do not restore a separate Lists
  title bar or replace the tabs with a dropdown.

- Write conventional, fully readable code. Never compress multiple statements,
  branches, declarations, or error checks onto one line. Use descriptive names,
  explicit control flow, and focused helpers.
- Format changed source, run `git diff --check`, and inspect the final diff before
  considering a change complete.
