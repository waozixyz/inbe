# Sync recovery

Sync enablement records the user’s intent, not whether the last request succeeded.
The 1.9.19 connection setting is migrated without enabling disconnected accounts.
Restoring a key explicitly requests synchronization with the configured server.
Restoring the same key preserves upload cursors, migration completion, and local data.

Connect and Retry schedule the existing background worker. Temporary challenge and
request failures retry after 5, 15, 30, then at most 60 seconds. The last result and
attempt count survive restart; authentication, configuration, signing, and payload
failures require action. Logout and server changes disable synchronization and
clear retry state. Queued records remain until the server response is applied.

Data and Sync show actionable status and queued changes. Repair flags and server
cursors stay internal. Secure-data migration runs through the normal background
sync path, without an automatic blocking prompt, and completion is only recorded
after successful response application.

Friendships are retrieved from the server under the restored account identity.
Successful synchronization schedules social refresh. Temporary social failures
retry; failed leaderboard requests do not discard a fetched friend list or replace
cached leaderboard data with empty results. Restoring a different account cannot
recover friendships belonging to the original identity.

Validation: `make sync-recovery-test`, the sync account/review and locale tests,
and `DAOCHI_BIN=/path/to/daochi make sync-server-test`. The server test creates two
accounts, accepts a friendship, restores an exported key on a fresh client, and
checks that both data and the existing friendship return. It also drops an upload
response to verify safe retry. Native screenshot scenes `sync_disconnected` and
`sync_retry` use disposable data and loopback configuration.
