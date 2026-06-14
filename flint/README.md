# Inbe Flint UI

This directory is first-party Inbe UI/runtime support code. It was merged from
the old Flint project so Inbe can extend the UI system directly without a
vendored submodule, project manifest, CLI, or separate library target.

## Layout

- `include/` - public headers used by Inbe source files
- `src/` - C implementation files compiled directly into Inbe
- `icons/` - icon bitmaps embedded by the Flint icon asset source
- `scripts/` - small build helpers, currently asset embedding

The root `Makefile` and Android CMake project compile these files directly.
