# Build contracts and Bend proofs

## Implemented

Inbe's release metadata uses the numeric changelog release and the permanent
`APP_VERSION_*` macros. `update_version.sh` synchronizes the release fields;
`check-version.py` rejects inconsistencies. The version check now runs in the
native, web, and Windows artifact graphs and in both Gradle and CMake, including
incremental builds. Version semantics remain a structural build check, not a
formal theorem.

The retry implementation is now **Bend 2 code that ships as evaluated data**:

1. `laws/sync_retry/main.bend` defines the actual pure retry policy.
2. `LAWS.bend` states 11 universally quantified contracts; `PROOF.bend` proves
   them for the complete finite input types.
3. Kryon's `tools/bend-laws.mjs` uses pinned upstream Bend 2.0.16, commit
   `15ae0c86f3193b8f645b4bedbc438655b648d0da`. It verifies the checker and Base
   hashes before loading, checks termination and types, and rejects holes,
   unproved declarations, `@unsafe`, foreign implementations, remote imports,
   and imports outside the proof package.
4. The checked function is evaluated directly in Bend's kernel for all 45
   combinations of result and retry state. `scripts/generate-sync-retry.mjs`
   writes the C table consumed by `src/app/sync_retry.kry`. There is no separate
   handwritten retry implementation or optimizing Bend code generator in this
   path. No Bend runtime is installed on the phone.
5. Account-data and social retries both use that table. The small adapter
   clamps persisted integers before indexing, avoiding increment overflow.
   Unknown results stop automatic retries.

The proofs establish success resetting the retry state; temporary challenge
and request failures following 5/15/30/60 seconds with a saturated fourth
attempt; actionable failures stopping automatic retry; and the mapping of
saved retry states and delays. Each error classification has its own law.
They do not prove server availability, delivery, alias/friend restoration,
or arbitrary `.kry` behavior. Existing integration tests cover those effects.

The Android private-data directory choice is also a proved finite Bend policy
in `laws/storage_layout`. Its generated table is consumed by `data_root()` and
distinguishes fresh installs, successful moves, failed moves, and an existing
`inbe` directory that must be archived before migration. The proof does not
establish the effects of Android filesystem calls or SQLite backup; storage
integration tests and a device migration check cover those effects. See
`docs/DATA_PATH_MIGRATION.md`.

## Build behavior and verification

Install Node.js **22.18 or newer** on the build host and initialize recursive
submodules. The checker runs offline once those dependencies are present.
CI and package builders provide Node explicitly. The container setup script
uses pinned, checksummed official Node archives; ordinary compilation never
downloads a proof tool. Flatpak removes its Node build tool from the app.

- `make build-laws`: release consistency and proofs.
- `make proof-test`: deterministic generation, contract-breaking mutations,
  enum mapping drift, and rejection with an old header already present.
- `make sync-recovery-test`: the actual generated C and coordinator, including
  all known result codes, extreme persisted integers, and social retry state.
- `make version-test`: release synchronization and rejecting mismatches.
- Upstream `make bend-laws-test`: checker/evaluator acceptance and rejection,
  including missing proofs, holes, unsafe definitions, and escaped imports.

Make's actual generated-header and app artifact prerequisites run the checks
on every invocation. Failed checking prevents compilation even if an old
header exists. Successful generation preserves the header's timestamp when
its contents are unchanged. Gradle `preBuild` always checks; direct CMake
builds independently regenerate their checked table. `make kry-c-plan9`
exports the checked table with the generated sources for native Plan 9,
which consumes the host-verified export.

Kryon's syntax law `image.surface.no_low_level_calls` remains mandatory in
all six compiler frontends, including default and `--no-strict` builds. Its
105 rejection cases are compiler regression tests, not behavioral proofs.
The text/button scanners, migration tests, and sync integration suite remain
separate checks with their own scope.

## Trust and use with smaller models

A model can change the implementation and provide proofs while the contracts
stay fixed. Its output is accepted only when the checker verifies those
contracts. This reduces dependence on model quality for **specified behavior**;
it does not establish that a smaller model is equally good at designing the
contracts or diagnosing missing requirements.

The trusted boundary is the pinned checker/Base, the finite-table generator,
the enum/integer adapter, build graph, and target compiler. Regression tests
exercise the integration boundary. An agent able to rewrite the laws or gates
can weaken the guarantee. Protected branches, required checks, and independent
review ownership should protect those files on the hosting service; repository
files alone cannot make themselves immutable. Hosting settings have not been
changed by this implementation.

`AGENTS.md` now states ownership, contract-change boundaries, and the remaining
product/review obligations instead of treating prose as a proof system.

## Next migrations, in order

1. Move practice lifecycle decisions into a pure finite policy: desktop focus
   cannot pause a session, user pause remains explicit, and mobile background
   transitions preserve the required timer behavior. Compile its proved table
   through the same Kryon checker; retain platform integration tests.
2. Specify sync recovery transitions: restoring an account cannot discard
   queued local changes, partial social refresh cannot erase known friends,
   and an account switch cannot apply responses belonging to the old account.
   Model effects explicitly; proofs of scheduling do not prove network delivery.
3. For unbounded algorithms such as collection merges, define a checked direct
   compilation path and its semantics before expanding beyond finite tables.
   Do not claim a proof of a model also proves a separately implemented C path.
4. Add typed widget contracts in Kryon's parsed compiler representation and
   negative fixtures before removing the remaining API instructions. Turning on
   today's whole-project strict mode produces thousands of unsupported-host
   diagnostics and is not a substitute for those contracts.

[Bend's upstream guide](https://github.com/bendlang/bend/blob/main/guide/GUIDE.md)
describes its law/proof convention. Inbe uses the actual checker rather than
naming ordinary assertions or tests mathematical proofs.
