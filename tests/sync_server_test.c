#include "storage.h"
#include "sync_account.h"
#include "sync_client.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static char current_root[1024];
void data_init(void) {}
const char *data_root(void) { return current_root; }
void TraceLog(int level, const char *fmt, ...) {
    (void)level;
    va_list args; va_start(args, fmt); vfprintf(stderr, fmt, args); va_end(args);
    fputc('\n', stderr);
}
static void open_client(const char *root) {
    storage_close();
    snprintf(current_root, sizeof(current_root), "%s", root);
    assert(storage_init(root));
}
static void synchronize(const char *url) {
    for(int attempt = 0; attempt < 8; attempt++) {
    SyncResult result = sync_client_connect(url);
    if(result != SYNC_OK) {
        fprintf(stderr, "sync failed: %s\n", GetSyncResultName(result));
        exit(1);
    }
    if(storage_sync_review_pending()) {
        assert(storage_sync_review_apply_remote_if_local_empty());
        assert(!storage_sync_review_pending());
    }
    InbeStorageSyncStatus status;
    assert(storage_sync_status(&status));
    if(!status.queued_changes && !status.secure_migration_pending) return;
    }
    fprintf(stderr, "sync did not drain queued changes\n");
    exit(1);
}
int main(int argc, char **argv) {
    SyncAccount account = {0};
    char a[1024], b[1024], backup[1024], restored[1024], id[256], other_id[256];
    int duration = 60;
    InbeStorageSessionCheckin checkin = {0}, loaded = {0};
    assert(argc == 4);
    /* The runner supplies a fresh loopback server and disposable data root. */
    assert(strncmp(argv[1], "http://127.0.0.1:", 17) == 0);
    assert(strncmp(argv[3], "http://127.0.0.1:", 17) == 0);
    snprintf(a, sizeof(a), "%s/a", argv[2]);
    snprintf(b, sizeof(b), "%s/b", argv[2]);
    snprintf(restored, sizeof(restored), "%s/restored", argv[2]);
    snprintf(backup, sizeof(backup), "%s/backup.zip", argv[2]);
    open_client(a);
    assert(sync_account_generate(&account));
    assert(sync_account_save(&account, 0) == INBE_SYNC_ACCOUNT_SAVE_OK);
    assert(storage_save_session_for_activity(&duration, 1, 0, 1, id, sizeof(id)));
    checkin.mood_after = 4;
    assert(storage_save_session_checkin(id, &checkin));
    assert(sync_client_connect("http://127.0.0.1:1") != SYNC_OK);
    assert(storage_session_count() == 1); /* Offline failure retains local work. */
    open_client(a);
    assert(storage_load_session_checkin(id, &loaded) && loaded.mood_after == 4);
    /* Server commits the upload, but the proxy drops its response. */
    assert(sync_client_connect(argv[3]) != SYNC_OK);
    open_client(a);
    synchronize(argv[1]);
    open_client(b);
    assert(sync_account_save(&account, 0) == INBE_SYNC_ACCOUNT_SAVE_OK);
    synchronize(argv[1]);
    assert(storage_session_count() == 1);
    assert(storage_load_session_checkin(id, &loaded) && loaded.mood_after == 4);
    checkin.mood_after = 2;
    assert(storage_save_session_checkin(id, &checkin));
    synchronize(argv[1]);
    open_client(a);
    synchronize(argv[1]);
    assert(storage_load_session_checkin(id, &loaded) && loaded.mood_after == 2);
    synchronize(argv[1]);
    assert(storage_session_count() == 1); /* Retrying an acknowledged upload is idempotent. */
    /* Both clients edit offline; A's second edit has the newer logical timestamp. */
    open_client(b);
    checkin.mood_after = 1;
    assert(storage_save_session_checkin(id, &checkin));
    open_client(a);
    checkin.mood_after = 3;
    assert(storage_save_session_checkin(id, &checkin));
    checkin.mood_after = 4;
    assert(storage_save_session_checkin(id, &checkin));
    open_client(b);
    synchronize(argv[1]);
    open_client(a);
    synchronize(argv[1]);
    open_client(b);
    synchronize(argv[1]);
    assert(storage_load_session_checkin(id, &loaded) && loaded.mood_after == 4);
    assert(storage_export_zip(backup));
    open_client(restored);
    assert(storage_import_zip(backup));
    assert(storage_session_count() == 1);
    assert(storage_load_session_checkin(id, &loaded) && loaded.mood_after == 4);
    open_client(b);
    duration = 90;
    assert(storage_save_session_for_activity(&duration, 1, 0, 1, other_id, sizeof(other_id)));
    assert(storage_delete_session(id));
    synchronize(argv[1]);
    open_client(a);
    synchronize(argv[1]);
    assert(storage_session_count() == 1); /* Deletion must preserve the unrelated upload. */
    assert(storage_load_session_checkin(other_id, &loaded));
    assert(storage_delete_session(other_id));
    synchronize(argv[1]);
    open_client(b);
    synchronize(argv[1]);
    assert(storage_session_count() == 0);
    storage_close();
    puts("PASS two-client sync, offline recovery, interrupted upload, conflict, mood, retry, backup restore, deletion");
    return 0;
}
