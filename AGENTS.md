# Inbe repository rules

## Ownership and releases

- Never edit `vendor/`. Change Kryon in its upstream repository on `master`,
  commit and push there, then update only Inbe's clean submodule pointer.
  Run `make no-vendor-edits` after dependency changes.
- Add the numeric release to `CHANGELOG.md`, then run `./update_version.sh`.
  The canonical macros are `APP_VERSION_STRING`, `APP_VERSION_MAJOR`,
  `APP_VERSION_MINOR`, and `APP_VERSION_PATCH`; do not create aliases.
  Only the GitHub Actions release workflow creates or pushes release tags.
- Translate changed strings in every affected locale; no English placeholders.

## Machine-checked contracts

- `make build-laws` is mandatory for native, web, and Windows artifacts;
  Gradle and CMake enforce the same version and proof checks independently.
  Never bypass a failed gate or hand-edit generated policy tables.
- Retry behavior lives in `laws/sync_retry/main.bend`. Implement changes there
  and prove `LAWS.bend` in `PROOF.bend`; the app consumes the checked table.
  Do not weaken laws to make an implementation pass. Contract changes require
  explicit product intent and separate review from implementation changes.
- Run `make proof-test sync-recovery-test version-test` for these contracts.
  Coverage, trust boundaries, and the next proof migrations are described in
  [docs/build-laws-plan.md](docs/build-laws-plan.md).

## Public UI and sync boundaries

- Use `Text(TextProps)`, `Image(ImageProps)`, and `Button(ButtonProps)` directly.
  Semantic images use the same `Image` surface. No positional Text, legacy
  drawing calls, alternate widgets, or thin local forwarding wrappers.
  Keep meaningful compositions and gestures; use standard Checkbox and Toggle.
  Run `make clean-text-api-check button-api-check` for UI changes.
- If a required widget capability is missing, implement the reusable primitive
  upstream first. Do not work around it in Inbe or generated code.
- Use sync protocol v6 and the clean Kryon API; no new `Ksync` names or wrappers.
  Keep legacy wire strings only for migration. Migrations must be automatic,
  resumable, and non-destructive, preserving tombstones and legacy projections
  while supported older installations can return.

## Product behavior and readability

- Desktop focus loss must not pause practices, freeze animation, suppress
  updates, or enter the mobile background-timer path. Preserve mobile/web
  lifecycle handling and explicit user pause as separate behavior.
- Preserve hover, press, and focus transitions. Session audio controls have
  44-unit targets (24-unit icons plus 10-unit padding), scaled and spaced.
- Lists starts with list tabs, without a separate title bar or dropdown.
- Use descriptive names, explicit control flow, and focused helpers. Never
  compress multiple statements, branches, declarations, or checks onto one line.
  Format changed source, inspect the final diff, and run `git diff --check`.
