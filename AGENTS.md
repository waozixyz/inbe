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

## Readability Rule

- Write conventional, fully readable code. Never compress multiple statements,
  branches, declarations, or error checks onto one line. Use descriptive names,
  explicit control flow, and focused helpers.
- Format changed source, run `git diff --check`, and inspect the final diff before
  considering a change complete.
