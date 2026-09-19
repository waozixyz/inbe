# Sync compatibility validation — 2026-09-14

## Production repair

The backend was restarting because it required `DAOCHI_TOKEN_SECRET_HEX`
while the deployed environment still used `KSYNC_TOKEN_SECRET_HEX`.
The configuration migration preserved the existing token secret and database
settings, retained the old settings for rollback, and made a private backup.

Daochi commit `ae40c7f` is deployed. Sync responses no longer advertise backend
protocol development as a client upgrade. `latest_protocol` echoes the client
protocol on typed sync responses; `server_latest_protocol` reports backend
capability separately. The opaque encrypted endpoint omits `latest_protocol`.

## Passed checks

- Daochi `make test`, including real ML-DSA authentication with the shipped
  Inbe, Ksync, and Daochi signing contexts and headers; altered signatures
  rejected; sync protocols v1–v5 accepted without upgrade warnings.
- Environment migration tests: credential/database preservation, idempotence,
  and rejection of conflicting old/new settings.
- Inbe sync-account and sync-review tests, including persisted newer protocol
  metadata that must not become an app-update notification.
- Inbe two-client integration test against a disposable Daochi server:
  upload/download, offline recovery, interrupted upload, retry, conflicts,
  mood data, backup restoration, and deletion.
- Official GitHub release APK 1.9.19 (versionCode 121), installed alongside
  the existing debug app on the attached Moto E6 Play:
  imported a disposable old-format `.key`, connected to production,
  saved and uploaded one meditation session, reset only the fresh test app,
  re-imported the same key, and restored the session from production.
  The Data screen reported one session, queued zero, and Ready, without
  the protocol-driven “Upgrade Inner Breeze” warning.
- All five newly added sync status/error messages are present exactly once
  in all 12 locale files. Vendor trees remain clean; `git diff --check` passes.

## Remaining limits

- The phone validation uses the unchanged published APK. Its missing locale
  messages still display identifiers; the translated fixes are in 1.10.0
  source and are not published yet.
- Building the current Android working tree fails at the existing
  `PickerDialog` / `PickerDialogProps` call in notification settings, whose
  API is absent from the selected runtime. A new APK has not been validated.
- The full locale test also reports pre-existing missing profile translations
  and unused profile/list keys outside the sync changes.
- A backup produced by the current development client was rejected by the
  old APK as an invalid export. Key-based server restoration passed; do not
  claim forward compatibility of development backup archives.
- Production `/healthz` succeeds. `/readyz` still reports a separately
  configured direct-payment issuer key missing; database, token secret,
  and signature-verifier checks pass. Deployment preserved that existing
  readiness state rather than changing payment configuration.
- This validates the tested release and wire formats, not every historical
  binary, platform, or possible account dataset.
