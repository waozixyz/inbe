# Enforced build laws for Inbe 2.0.0

## Goal and current evidence

Turn objective rules into failing compiler or build checks, and keep
`AGENTS.md` short. A green build guarantees only its specified invariants;
it does not guarantee that every possible product behavior is correct.

Inbe remains the pending 2.0.0 release, Android versionCode 124. Version
metadata is synchronized by `update_version.sh`; this work does not create
a second release number or a release tag.

- Kryon commit `e89003b1` makes existing compiler laws mandatory in all six
  frontends, including ordinary compilation and `--no-strict`. Its regression
  tests reject 105 forbidden calls across compiler modes without emitting
  output. Compiler specification and runtime parity checks pass.
- Inbe now references that upstream commit. Its complete maintained `.kry`
  source transpiles with the mandatory laws enabled.
- A full strict-checking trial produced 25,409 diagnostics, predominantly
  unresolved names and unsupported C expressions. Enabling `--strict`
  everywhere is not an implementation of this plan.
- Version checks currently gate tests, packaging, and release CI. Several
  checks still do not gate native compilation or direct Gradle invocation.

## 1. Close build-entrypoint gaps

Add `scripts/check-build-laws.py` as the single non-mutating build preflight.
It invokes existing version and canonical-widget checks, checks submodule
cleanliness and public HTTPS URLs, and validates locale keys and nonempty
translations. Do not duplicate existing validators in the orchestrator.
Every diagnostic names the violated invariant and the offending file.

Run it before source generation and final artifacts in the native, web,
Windows, and Android build graphs. In Make, use order-only prerequisites on
the actual generation and artifact targets, not only convenience targets.
In Gradle, attach it to `preBuild` for debug, release, and gplay. Direct CMake
builds must also depend on the check before app generation and compilation.
The Plan 9 export runs the same checks on the host before emitting sources;
the native Plan 9 build consumes that checked export.

Run the preflight on incremental builds too. Do not accept a previously
written success marker after source, metadata, or vendor state changes.
Git-less source archives may validate content and version rules, but must
explicitly report that repository ownership checks cannot be evaluated;
release builds require a Git checkout for those checks.

## 2. Move reusable API rules into Kryon

Keep one declared, typed public signature for each widget. Extend the shared
compiler checks to enforce the Text, Image, and Button contracts before
lowering, independently of the full host-language type checker. Validate
arguments against the canonical declarations; do not create a second list
of signatures that can drift from them.

Add rejecting fixtures for positional Text, obsolete image calls, removed
widget aliases, and wrong props. Include accepting fixtures with named props,
comments, strings, and legitimate composition. Exercise C, C++, Go, JS, KIR,
and cartridge frontends. Backend implementation calls remain confined to
their owning runtime sources and are not public app widget APIs.

Leave Inbe-specific rules such as its version format, Lists layout, and
account recovery in Inbe. Make reusable Kryon changes on upstream `master`,
commit and push them, then move only Inbe's clean submodule pointer.

## 3. Enforce behavior at the appropriate level

| Invariant | Enforcement |
|---|---|
| Version fields agree; versionCode changes once | Build preflight and updater regression tests |
| Canonical widget signatures and forbidden app API calls | Compiler diagnostics before lowering |
| All locale keys have values | Build preflight; translation meaning still requires review |
| Temporary sync failure retains queued data | Sync state and integration tests in required CI |
| Same-key restoration recovers alias, friends, and data | Disposable-server integration test in release prerequisites |
| Leaderboard failure preserves friends | Social recovery regression test |
| Desktop focus loss does not pause a practice | Lifecycle regression test |
| No downstream vendor edits; submodules use HTTPS | Repository preflight |

Reuse existing tests where they already establish the invariant. Add tests
only for missing coverage. Network-dependent integration checks gate release
CI, while ordinary local compilation runs deterministic, offline checks.

## 4. Reduce instructions after enforcement exists

Replace long implemented API and version instructions in `AGENTS.md` with
short references to their canonical declarations, law IDs, and check command.
Keep repository ownership, upstream workflow, release-tag ownership, product
boundaries, and human review responsibilities. Do not delete an instruction
until its replacement check and a negative regression test are present.

Keep objective contracts and their tests separate from ordinary feature
changes. Configure required CI and review ownership for changes to laws and
release gates if repository administration is included in the rollout. A
modifiable local check alone is not an immutable rule.

## Acceptance and rollout

In disposable checkouts, introduce one violation at a time: a mismatched
version, duplicate version macro, missing locale value, forbidden widget
call, dirty vendor file, or SSH submodule URL. Every applicable build entry
point must fail before emitting a new app artifact, including an incremental
build that had previously succeeded. Restore the file and verify the build
can proceed. Leave unrelated changes and real account data untouched.

Then rebuild native, web, and Android targets, run the sync release checks,
and verify startup after an in-place Android update. Publish only through
the normal release workflow; never create a tag manually.

## Bend-style proof boundary

[Bend 2's guide](https://github.com/bendlang/bend/blob/main/guide/GUIDE.md)
describes laws discharged by proof terms. Kryon's current syntax checks and
runtime tests are not that proof system. This rollout does not add dependent
types, a new `law` syntax, or an external prover dependency.

A later proof project must first define a pure, fully typed `.kry` subset,
termination and integer semantics, and explicit foreign-function assumptions.
Proofs must refer to the implementation actually emitted, not a separately
maintained model. Until that design is complete, label behavior checks as
tests and do not claim mathematical proof of arbitrary app or network code.
