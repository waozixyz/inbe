#include "src/app/app_sync.c"
#include <assert.h>

void PushInspectSource(const char *path, int line)
{
    (void)path;
    (void)line;
}
void PopInspectSource(void) {}

static double test_time = 100;
static int last_result;
static int retry_attempt;
static int enabled = 1;
static StorageSyncStatus test_status;

double GetTime(void)
{
    return test_time;
}
void MutexLock(Mutex *mutex)
{
    (void)mutex;
}
void MutexUnlock(Mutex *mutex)
{
    (void)mutex;
}
int storage_get_setting_int(const char *key, int fallback)
{
    if(strcmp(key, "sync_last_result")
        == 0) return last_result;
    if(strcmp(key, "sync_retry_attempt")
        == 0) return retry_attempt;
    return fallback;
}
void storage_set_setting_int(const char *key, int value)
{
    if(strcmp(key, "sync_last_result")
        == 0) last_result = value;
    if(strcmp(key, "sync_retry_attempt")
        == 0) retry_attempt = value;
}
int storage_sync_enabled(void)
{
    return enabled;
}
int storage_sync_review_pending(void)
{
    return 0;
}
int storage_sync_status(StorageSyncStatus *status)
{
    *status = test_status;
    return 1;
}
SyncResult sync_client_send_friend_request(const char *url, const char *value)
{
    (void)url;
    (void)value;
    return SYNC_OK;
}
SyncResult sync_client_accept_friend_request(const char *url, const char *value)
{
    (void)url;
    (void)value;
    return SYNC_OK;
}
SyncResult sync_client_decline_friend_request(const char *url, const char *value)
{
    (void)url;
    (void)value;
    return SYNC_OK;
}
SyncResult sync_client_remove_friend(const char *url, const char *value)
{
    (void)url;
    (void)value;
    return SYNC_OK;
}
SyncResult sync_client_get_friend_requests(const char *url, char *out, size_t size)
{
    (void)url;
    snprintf(out, size, "{\"incoming\":[],\"outgoing\":[]}");
    return SYNC_OK;
}
SyncResult sync_client_get_friends(const char *url, char *out, size_t size)
{
    (void)url;
    snprintf(out, size, "{\"friends\":[{\"user_id_hash\":\"existing-friend\"}]}");
    return SYNC_OK;
}
SyncResult sync_client_get_friend_stats(const char *url, const char *app,
                                      const char *practice, const char *metric,
                                      char *out, size_t size)
{
    (void)url;
    (void)app;
    (void)practice;
    (void)metric;
    (void)out;
    (void)size;
    return SYNC_REQUEST_FAILED;
}
int main(void)
{
    const double delays[] = {5, 15, 30, 60, 60};
    for(unsigned i = 0; i < sizeof(delays) / sizeof(delays[0]); i++) {
        app_sync_record_result(SYNC_REQUEST_FAILED);
        assert(g_sync.retry_at == test_time + delays[i]);
        assert(enabled);
        assert(strcmp(app_sync_status_key(), "sync_retry_waiting") == 0);
        test_time = g_sync.retry_at;
    }
    app_sync_record_result(SYNC_AUTH_FAILED);
    assert(g_sync.retry_at == 0);
    assert(app_sync_failure_needs_action(last_result));
    assert(strcmp(app_sync_status_key(), "sync_auth_failed") == 0);
    app_sync_record_result(SYNC_OK);
    assert(retry_attempt == 0 && g_sync.retry_at == 0);
    test_status.queued_changes = 4745;
    test_status.repair_pending = 1;
    enabled = 0;
    assert(strcmp(app_sync_status_key(), "sync_connection_required") == 0);
    enabled = 1;
    g_sync.sync_running = 1;
    assert(strcmp(app_sync_status_key(), "sync_in_progress") == 0);
    g_sync.sync_running = 0;
    assert(strcmp(app_sync_status_key(), "sync_status_local_queued") == 0);
    test_status.queued_changes = 0;
    test_status.full_upload_done = 1;
    assert(strcmp(app_sync_status_key(), "sync_status_ready") == 0);
    char requests[256], friends[256], leaderboard[256];
    assert(app_social_fetch("http://127.0.0.1", "whm", "streak", "", "",
                            requests, sizeof(requests), friends, sizeof(friends),
                            leaderboard, sizeof(leaderboard)) == SYNC_OK);
    assert(strstr(friends, "existing-friend") != NULL);
    assert(leaderboard[0] == '\0');
    puts("PASS sync retry backoff, actionable errors, completion and disconnected status");
    return 0;
}
