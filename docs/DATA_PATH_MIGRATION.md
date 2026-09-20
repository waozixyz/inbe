# Private data path migration

The canonical root is `<app internalDataPath>/inbe` on Android,
`$XDG_DATA_HOME/inbe` (or `~/.local/share/inbe`) on Unix desktops,
`%LOCALAPPDATA%/inbe` on Windows, and `/home/inbe` in the web filesystem.
The database is `inbe.db` inside that directory. Android release and debug
packages each have their own private `internalDataPath`. Existing export
archive names remain compatible.

On startup, if the legacy `breathing` directory exists (or `BreathSession` on
Windows), the app moves the whole
directory before opening SQLite. This keeps the database, WAL, SHM, and any
other files together. An older `inbe` directory is archived under
`inbe.before-breathing-migration-N` first, without overwriting it. If either
directory move fails, the app continues using `breathing` and can retry next
startup.

When `breathing.db` exists, SQLite's backup API copies its committed main and
WAL content into a temporary database. The copy must pass `PRAGMA quick_check`
before it becomes `inbe.db`. A migration marker identifies the completed copy;
the old database remains as a recovery copy. If the copy fails, the app uses
`breathing.db`. If both database names exist without the marker, startup fails
closed rather than choosing one silently.

`laws/storage_layout/LAWS.bend` defines the finite directory choice policy:

- A fresh install uses the current directory.
- A successful legacy move uses the current directory.
- A failed move keeps using the legacy directory.
- If both exist at decision time, the legacy directory is used. The runtime
  first archives the previous `inbe` directory; the policy then sees the
  current directory as absent and applies the move rule.

The generated table from the proved Bend policy is consumed by `data_root()`.
The proof covers these decisions, not filesystem atomicity or SQLite contents.
Storage integration tests cover pending WAL data, repeat startup, and database
name conflicts. The Android test covers both-directory archival and compares
the user-data tables before and after migration.
# Desktop profiles

`make run` and `make run-termi` use the persistent debug profile at
`$XDG_DATA_HOME/inbe-debug` (or `~/.local/share/inbe-debug` when XDG_DATA_HOME
is unset). `make run-fresh` still uses a disposable temporary profile.

`make install` installs the production desktop app under `~/.local` by default.
Launching `inbe` or its desktop entry uses `$XDG_DATA_HOME/inbe` (or
`~/.local/share/inbe`). Sync keys, settings, sessions, habits, and downloaded
meditation audio stay inside their respective profile directories. `PREFIX` can
be set explicitly for a system installation; it does not change the production
data location. Debug and production windows have separate desktop identities and
single-instance locks, so both can stay open at once.
