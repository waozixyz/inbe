#ifndef INBE_SYNC_CLIENT_H
#define INBE_SYNC_CLIENT_H

#include <stddef.h>

typedef enum InbeSyncClientResult {
    INBE_SYNC_CLIENT_OK = 0,
    INBE_SYNC_CLIENT_INVALID_URL,
    INBE_SYNC_CLIENT_NO_ACCOUNT,
    INBE_SYNC_CLIENT_PAYLOAD_FAILED,
    INBE_SYNC_CLIENT_CHALLENGE_FAILED,
    INBE_SYNC_CLIENT_SIGN_FAILED,
    INBE_SYNC_CLIENT_REQUEST_FAILED,
    INBE_SYNC_CLIENT_AUTH_FAILED
} InbeSyncClientResult;

int sync_client_url_valid(const char *url);
int sync_client_normalize_url(const char *input, char *out, size_t out_size);
InbeSyncClientResult sync_client_sync(const char *base_url);
InbeSyncClientResult sync_client_register_alias(const char *base_url, const char *alias);
InbeSyncClientResult sync_client_wait_for_remote_event(const char *base_url);
InbeSyncClientResult sync_client_delete_account(const char *base_url);
const char *sync_client_result_name(InbeSyncClientResult result);
void sync_client_clear_auth_token(void);
#if defined(__EMSCRIPTEN__)
int sync_client_web_start_remote_events(const char *base_url);
int sync_client_web_poll_remote_event(void);
int sync_client_web_sync_start(const char *base_url);
int sync_client_web_sync_poll(InbeSyncClientResult *result, int *changed);
#endif

#endif
