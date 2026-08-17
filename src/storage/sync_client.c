#include "sync_client.h"

#include "storage.h"
#include "sync_account.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INBE_SYNC_WS_PATH "/api/v1/sync/ws"
#define INBE_ACCOUNT_ALIAS_PATH "/api/v1/account/alias"
#define INBE_FRIENDS_PATH "/api/v1/friends"
#define INBE_FRIEND_REQUESTS_PATH "/api/v1/friends/requests"
#define INBE_FRIEND_STATS_PATH "/api/v1/friends/stats"

static KsyncSyncResult sync_client_bearer_request(const char *base_url,
                                                  const char *method,
                                                  const char *path,
                                                  const char *body,
                                                  char *out,
                                                  size_t out_size);

static void
sync_log_http_failure(const char *step, long status, const char *response)
{
    char snippet[161];
    size_t len;

    if(response == NULL)
        response = "";
    len = strlen(response);
    if(len >= sizeof(snippet))
        len = sizeof(snippet) - 1;
    memcpy(snippet, response, len);
    snippet[len] = '\0';
    for(size_t i = 0; i < len; i++) {
        if(snippet[i] == '\n' || snippet[i] == '\r' || snippet[i] == '\t')
            snippet[i] = ' ';
    }
    TraceLog(LOG_WARNING, "SYNC: %s failed status=%ld response=%s", step, status, snippet);
}

int
sync_client_url_valid(const char *url)
{
    return IsKsyncSyncURLValid(url);
}

int
sync_client_normalize_url(const char *input, char *out, size_t out_size)
{
    return NormalizeKsyncSyncURL(input, out, out_size);
}

static int
sync_client_is_hex64(const char *text)
{
    if(text == NULL || strlen(text) != 64)
        return 0;
    for(int i = 0; i < 64; i++) {
        char ch = text[i];
        if(!((ch >= '0' && ch <= '9') ||
             (ch >= 'a' && ch <= 'f') ||
             (ch >= 'A' && ch <= 'F')))
            return 0;
    }
    return 1;
}

int
sync_client_normalize_friend_target(const char *target, char *out, size_t out_size)
{
    char alias[40];
    int n = 0;
    int start = 0;

    if(out == NULL || out_size == 0)
        return 0;
    out[0] = '\0';
    if(target == NULL)
        return 0;
    while(target[start] == ' ' || target[start] == '\t' ||
          target[start] == '\n' || target[start] == '\r')
        start++;
    if(target[start] == '@')
        start++;

    if(sync_client_is_hex64(target + start)) {
        if(out_size < 65)
            return 0;
        for(int i = 0; i < 64; i++) {
            char ch = target[start + i];
            out[i] = (ch >= 'A' && ch <= 'F') ? (char)(ch - 'A' + 'a') : ch;
        }
        out[64] = '\0';
        return 1;
    }

    for(int i = start; target[i] != '\0' && n < (int)sizeof(alias) - 1; i++) {
        unsigned char ch = (unsigned char)target[i];
        if(ch >= 'A' && ch <= 'Z')
            ch = (unsigned char)(ch - 'A' + 'a');
        if((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_')
            alias[n++] = (char)ch;
        else if(ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r')
            break;
        else
            return 0;
    }
    alias[n] = '\0';
    if(n < 4 || n > 32 || out_size < (size_t)n + 2)
        return 0;
    snprintf(out, out_size, "@%s", alias);
    return 1;
}

#if defined(INBE_SYNC_CLIENT_TESTS)
int
sync_client_test_response_buffer(const char *first, const char *second,
                                 char *out, size_t out_size)
{
    KsyncSyncBuffer buffer = {0};
    int ok;

    if(out == NULL || out_size == 0)
        return 0;
    out[0] = '\0';
    ok = AppendKsyncSyncBuffer(&buffer, first, first != NULL ? strlen(first) : 0) &&
         AppendKsyncSyncBuffer(&buffer, second, second != NULL ? strlen(second) : 0);
    if(ok && buffer.data != NULL)
        snprintf(out, out_size, "%s", buffer.data);
    FreeKsyncSyncBuffer(&buffer);
    return ok;
}

int
sync_client_test_friend_request_body(const char *target, char *out, size_t out_size)
{
    KsyncSyncBuffer body = {0};
    char normalized[80];
    int ok;

    if(out == NULL || out_size == 0)
        return 0;
    out[0] = '\0';
    if(!sync_client_normalize_friend_target(target, normalized, sizeof(normalized)))
        return 0;
    ok = AppendKsyncSyncBuffer(&body, "{\"target\":", strlen("{\"target\":")) &&
         AppendKsyncSyncBufferJSONString(&body, normalized) &&
         AppendKsyncSyncBuffer(&body, "}", 1);
    if(ok && body.data != NULL && strlen(body.data) < out_size)
        snprintf(out, out_size, "%s", body.data);
    else
        ok = 0;
    FreeKsyncSyncBuffer(&body);
    return ok;
}
#endif

static const char *
sync_kryon_get_text(const char *key, void *user)
{
    (void)user;
    return storage_get_setting_text(key);
}

static void
sync_kryon_set_text(const char *key, const char *value, void *user)
{
    (void)user;
    storage_set_setting_text(key, value);
}

static char *
sync_kryon_build_payload(const char *user_id_hash, const char *public_key_hex,
                         void *user)
{
    (void)user;
    return storage_build_sync_payload_json(user_id_hash, public_key_hex);
}

static void
sync_kryon_free_payload(char *payload, void *user)
{
    (void)user;
    storage_free_sync_payload_json(payload);
}

static int
sync_kryon_apply_response(const char *response_json, void *user)
{
    (void)user;
    return storage_apply_sync_response_json(response_json);
}

static void
sync_kryon_purge_synced_deleted(void *user)
{
    (void)user;
    storage_purge_synced_deleted_data();
}

static void
sync_kryon_log_http_failure(const char *step, long status,
                            const char *response, void *user)
{
    (void)user;
    sync_log_http_failure(step, status, response);
}

static KsyncSyncConfig
sync_kryon_config(const char *base_url, const KsyncAccount *account)
{
    KsyncSyncConfig cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.base_url = base_url;
    cfg.account = account;
    cfg.client_id = storage_sync_client_id();
    cfg.http_request = KsyncDefaultHttpRequest;
    cfg.get_text = sync_kryon_get_text;
    cfg.set_text = sync_kryon_set_text;
    cfg.build_payload = sync_kryon_build_payload;
    cfg.free_payload = sync_kryon_free_payload;
    cfg.apply_response = sync_kryon_apply_response;
    cfg.purge_synced_deleted = sync_kryon_purge_synced_deleted;
    cfg.log_http_failure = sync_kryon_log_http_failure;
    return cfg;
}

void
sync_client_clear_auth_token(void)
{
    KsyncSyncConfig cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.set_text = sync_kryon_set_text;
    ClearKsyncSyncAuthToken(&cfg);
}

KsyncSyncResult
sync_client_sync(const char *base_url)
{
    KsyncAccount account;
    KsyncSyncConfig cfg;

    if(!sync_client_url_valid(base_url))
        return KSYNC_SYNC_INVALID_URL;
    if(!sync_account_load(&account))
        return KSYNC_SYNC_NO_ACCOUNT;
    cfg = sync_kryon_config(base_url, &account);
    return RunKsyncSync(&cfg);
}

KsyncSyncResult
sync_client_register_alias(const char *base_url, const char *alias)
{
    KsyncAccount account;
    KsyncSyncBuffer body = {0};
    KsyncSyncResult result;
    char response[512];
    char saved_alias[40];

    if(!sync_client_url_valid(base_url))
        return KSYNC_SYNC_INVALID_URL;
    if(alias == NULL || alias[0] == '\0')
        return KSYNC_SYNC_PAYLOAD_FAILED;
    if(!sync_account_load(&account))
        return KSYNC_SYNC_NO_ACCOUNT;

    if(!AppendKsyncSyncBuffer(&body, "{\"user_id_hash\":", strlen("{\"user_id_hash\":")) ||
       !AppendKsyncSyncBufferJSONString(&body, account.public_id) ||
       !AppendKsyncSyncBuffer(&body, ",\"alias\":", strlen(",\"alias\":")) ||
       !AppendKsyncSyncBufferJSONString(&body, alias) ||
       !AppendKsyncSyncBuffer(&body, "}", 1)) {
        FreeKsyncSyncBuffer(&body);
        return KSYNC_SYNC_PAYLOAD_FAILED;
    }
    TraceLog(LOG_INFO, "SYNC: alias request alias=@%s", alias);
    result = sync_client_bearer_request(base_url, "POST", INBE_ACCOUNT_ALIAS_PATH,
                                        body.data, response, sizeof(response));
    FreeKsyncSyncBuffer(&body);
    if(result != KSYNC_SYNC_OK)
        return result;
    if(FindKsyncSyncJSONString(response, "alias", saved_alias, sizeof(saved_alias))) {
        storage_set_setting_text("sync_account_alias", saved_alias);
        TraceLog(LOG_INFO, "SYNC: alias response stored @%s", saved_alias);
    } else {
        TraceLog(LOG_WARNING, "SYNC: alias response missing alias field");
    }
    return KSYNC_SYNC_OK;
}

static KsyncSyncResult
sync_client_bearer_request(const char *base_url, const char *method, const char *path,
                           const char *body, char *out, size_t out_size)
{
    KsyncAccount account;
    KsyncSyncConfig cfg;

    if(!sync_client_url_valid(base_url))
        return KSYNC_SYNC_INVALID_URL;
    if(!sync_account_load(&account))
        return KSYNC_SYNC_NO_ACCOUNT;
    cfg = sync_kryon_config(base_url, &account);
    return RequestKsyncSyncBearer(&cfg, method, path, body, out, out_size);
}

KsyncSyncResult
sync_client_send_friend_request(const char *base_url, const char *target)
{
    KsyncSyncBuffer body = {0};
    KsyncSyncResult result;
    char normalized[80];

    if(!sync_client_normalize_friend_target(target, normalized, sizeof(normalized)))
        return KSYNC_SYNC_PAYLOAD_FAILED;
    if(!AppendKsyncSyncBuffer(&body, "{\"target\":", strlen("{\"target\":")) ||
       !AppendKsyncSyncBufferJSONString(&body, normalized) ||
       !AppendKsyncSyncBuffer(&body, "}", 1)) {
        FreeKsyncSyncBuffer(&body);
        return KSYNC_SYNC_PAYLOAD_FAILED;
    }
    TraceLog(LOG_INFO, "SYNC: friend request target=%s", normalized);
    result = sync_client_bearer_request(base_url, "POST", INBE_FRIEND_REQUESTS_PATH,
                                        body.data, NULL, 0);
    FreeKsyncSyncBuffer(&body);
    return result;
}

KsyncSyncResult
sync_client_get_friend_requests(const char *base_url, char *out, size_t out_size)
{
    return sync_client_bearer_request(base_url, "GET", INBE_FRIEND_REQUESTS_PATH,
                                      NULL, out, out_size);
}

KsyncSyncResult
sync_client_get_friends(const char *base_url, char *out, size_t out_size)
{
    return sync_client_bearer_request(base_url, "GET", INBE_FRIENDS_PATH,
                                      NULL, out, out_size);
}

static KsyncSyncResult
sync_client_friend_request_action(const char *base_url, const char *request_id,
                                  const char *action)
{
    char path[256];

    if(request_id == NULL || request_id[0] == '\0' || action == NULL)
        return KSYNC_SYNC_PAYLOAD_FAILED;
    if(snprintf(path, sizeof(path), "%s/%s/%s", INBE_FRIEND_REQUESTS_PATH,
                request_id, action) >= (int)sizeof(path))
        return KSYNC_SYNC_PAYLOAD_FAILED;
    return sync_client_bearer_request(base_url, "POST", path, "{}", NULL, 0);
}

KsyncSyncResult
sync_client_accept_friend_request(const char *base_url, const char *request_id)
{
    return sync_client_friend_request_action(base_url, request_id, "accept");
}

KsyncSyncResult
sync_client_decline_friend_request(const char *base_url, const char *request_id)
{
    return sync_client_friend_request_action(base_url, request_id, "decline");
}

KsyncSyncResult
sync_client_remove_friend(const char *base_url, const char *friend_user_id)
{
    char path[192];

    if(friend_user_id == NULL || friend_user_id[0] == '\0')
        return KSYNC_SYNC_PAYLOAD_FAILED;
    if(snprintf(path, sizeof(path), "%s/%s", INBE_FRIENDS_PATH,
                friend_user_id) >= (int)sizeof(path))
        return KSYNC_SYNC_PAYLOAD_FAILED;
    return sync_client_bearer_request(base_url, "DELETE", path, NULL, NULL, 0);
}

static int
sync_url_append_query(char *url, size_t url_size, const char *key, const char *value)
{
    const char *p;
    char sep = strchr(url, '?') == NULL ? '?' : '&';

    if(strlen(url) + strlen(key) + 2 >= url_size)
        return 0;
    strncat(url, &sep, 1);
    strncat(url, key, url_size - strlen(url) - 1);
    strncat(url, "=", url_size - strlen(url) - 1);
    if(value == NULL)
        value = "";
    for(p = value; *p != '\0'; p++) {
        char encoded[4];
        if((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
           (*p >= '0' && *p <= '9') || *p == '_' || *p == '-' ||
           *p == '.' || *p == ':') {
            if(strlen(url) + 1 >= url_size)
                return 0;
            strncat(url, p, 1);
        } else {
            if(strlen(url) + 3 >= url_size)
                return 0;
            snprintf(encoded, sizeof(encoded), "%%%02X", (unsigned char)*p);
            strncat(url, encoded, url_size - strlen(url) - 1);
        }
    }
    return 1;
}

KsyncSyncResult
sync_client_get_friend_stats(const char *base_url, const char *app,
                             const char *practice, const char *metric,
                             char *out, size_t out_size)
{
    char path[512];

    snprintf(path, sizeof(path), "%s", INBE_FRIEND_STATS_PATH);
    if(!sync_url_append_query(path, sizeof(path), "app", app != NULL ? app : "inbe") ||
       !sync_url_append_query(path, sizeof(path), "practice", practice) ||
       !sync_url_append_query(path, sizeof(path), "metric", metric))
        return KSYNC_SYNC_PAYLOAD_FAILED;
    return sync_client_bearer_request(base_url, "GET", path, NULL, out, out_size);
}

#if defined(INBE_SYNC_CLIENT_TESTS)
int
sync_client_test_friend_stats_path(const char *app, const char *practice,
                                   const char *metric, char *out, size_t out_size)
{
    char path[512];

    if(out == NULL || out_size == 0)
        return 0;
    out[0] = '\0';
    snprintf(path, sizeof(path), "%s", INBE_FRIEND_STATS_PATH);
    if(!sync_url_append_query(path, sizeof(path), "app", app) ||
       !sync_url_append_query(path, sizeof(path), "practice", practice) ||
       !sync_url_append_query(path, sizeof(path), "metric", metric) ||
       strlen(path) >= out_size)
        return 0;
    snprintf(out, out_size, "%s", path);
    return 1;
}
#endif

KsyncSyncResult
sync_client_wait_for_remote_event(const char *base_url)
{
    KsyncAccount account;
    KsyncSyncConfig cfg;

    if(!sync_client_url_valid(base_url))
        return KSYNC_SYNC_INVALID_URL;
    if(!sync_account_load(&account))
        return KSYNC_SYNC_NO_ACCOUNT;
    cfg = sync_kryon_config(base_url, &account);
    return KsyncRemoteEventWait(&cfg, INBE_SYNC_WS_PATH);
}

KsyncSyncResult
sync_client_delete_account(const char *base_url)
{
    KsyncAccount account;
    KsyncSyncConfig cfg;

    if(!sync_client_url_valid(base_url))
        return KSYNC_SYNC_INVALID_URL;
    if(!sync_account_load(&account))
        return KSYNC_SYNC_NO_ACCOUNT;
    cfg = sync_kryon_config(base_url, &account);
    return DeleteKsyncSyncAccount(&cfg);
}

#if defined(__EMSCRIPTEN__)
int
sync_client_web_start_remote_events(const char *base_url)
{
    KsyncAccount account;
    KsyncSyncConfig cfg;

    if(!sync_client_url_valid(base_url))
        return 0;
    if(!sync_account_load(&account))
        return 0;
    cfg = sync_kryon_config(base_url, &account);
    return KsyncWebRemoteEventsStart(&cfg, INBE_SYNC_WS_PATH);
}

int
sync_client_web_poll_remote_event(void)
{
    return KsyncWebRemoteEventsPoll();
}

int
sync_client_web_sync_start(const char *base_url)
{
    KsyncAccount account;
    KsyncSyncConfig cfg;

    if(!sync_client_url_valid(base_url))
        return 0;
    if(!sync_account_load(&account))
        return 0;
    cfg = sync_kryon_config(base_url, &account);
    return KsyncWebSyncStart(&cfg);
}

int
sync_client_web_sync_poll(KsyncSyncResult *result, int *changed)
{
    int done;

    done = KsyncWebSyncPoll(result, changed);
    if(done && result != NULL && *result == KSYNC_SYNC_OK && changed != NULL)
        *changed = storage_last_sync_changed();
    return done;
}
#endif
