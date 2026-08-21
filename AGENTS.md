# Inbe Agent Instructions

- Never edit files under `vendor/` from this repository.
- Kryon changes must be made in the core Kryon repository, committed there, pushed there, and then brought into Inbe by updating the Kryon submodule pointer.
- Do not add tests, policy checks, wrappers, compatibility aliases, or generated changes inside `vendor/` from Inbe.
- If a task appears to require a vendor change, stop and switch to the upstream project workflow first.
- Run `make no-vendor-edits` or `make test` before handing work back when a task touches Kryon or other vendored code.
