#ifndef INBE_SYNC_CLIENT_H
#define INBE_SYNC_CLIENT_H

typedef enum InbeSyncClientResult {
    INBE_SYNC_CLIENT_OK = 0,
    INBE_SYNC_CLIENT_UNAVAILABLE,
    INBE_SYNC_CLIENT_INVALID_URL,
    INBE_SYNC_CLIENT_NO_ACCOUNT,
    INBE_SYNC_CLIENT_PAYLOAD_FAILED,
    INBE_SYNC_CLIENT_CHALLENGE_FAILED,
    INBE_SYNC_CLIENT_SIGN_FAILED,
    INBE_SYNC_CLIENT_REQUEST_FAILED,
    INBE_SYNC_CLIENT_AUTH_FAILED
} InbeSyncClientResult;

InbeSyncClientResult inbe_sync_client_sync(const char *base_url);
InbeSyncClientResult inbe_sync_client_delete_remote(const char *base_url);

#endif
