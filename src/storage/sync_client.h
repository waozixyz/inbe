#ifndef INBE_SYNC_CLIENT_H
#define INBE_SYNC_CLIENT_H

#if defined(__has_include)
#if __has_include("ksync_sync.h")
#include "ksync_sync.h"
#else
#include "lyra_sync.h"
#define KsyncSyncResult LyraSyncResult
#define KSYNC_SYNC_OK LYRA_SYNC_OK
#define KSYNC_SYNC_INVALID_URL LYRA_SYNC_INVALID_URL
#define KSYNC_SYNC_NO_ACCOUNT LYRA_SYNC_NO_ACCOUNT
#define KSYNC_SYNC_PAYLOAD_FAILED LYRA_SYNC_PAYLOAD_FAILED
#define KSYNC_SYNC_CHALLENGE_FAILED LYRA_SYNC_CHALLENGE_FAILED
#define KSYNC_SYNC_SIGN_FAILED LYRA_SYNC_SIGN_FAILED
#define KSYNC_SYNC_REQUEST_FAILED LYRA_SYNC_REQUEST_FAILED
#define KSYNC_SYNC_AUTH_FAILED LYRA_SYNC_AUTH_FAILED
#define KsyncSyncBuffer LyraSyncBuffer
#define KsyncSyncConfig LyraSyncConfig
#define AppendKsyncSyncBuffer AppendLyraSyncBuffer
#define AppendKsyncSyncBufferJSONString AppendLyraSyncBufferJSONString
#define FreeKsyncSyncBuffer FreeLyraSyncBuffer
#define FindKsyncSyncJSONString FindLyraSyncJSONString
#define FindKsyncSyncJSONInt64 FindLyraSyncJSONInt64
#define GetKsyncSyncResultName GetLyraSyncResultName
#define IsKsyncSyncURLValid IsLyraSyncURLValid
#define NormalizeKsyncSyncURL NormalizeLyraSyncURL
#define JoinKsyncSyncURL JoinLyraSyncURL
#define JoinKsyncSyncWebSocketURL JoinLyraSyncWebSocketURL
#define ClearKsyncSyncAuthToken ClearLyraSyncAuthToken
#define LoginKsyncSync LoginLyraSync
#define RunKsyncSync RunLyraSync
#define RequestKsyncSyncBearer RequestLyraSyncBearer
#define DeleteKsyncSyncAccount DeleteLyraSyncAccount
#endif
#else
#include "ksync_sync.h"
#endif

#include <stddef.h>

int sync_client_url_valid(const char *url);
int sync_client_normalize_url(const char *input, char *out, size_t out_size);
int sync_client_normalize_friend_target(const char *target, char *out, size_t out_size);
KsyncSyncResult sync_client_sync(const char *base_url);
KsyncSyncResult sync_client_register_alias(const char *base_url, const char *alias);
KsyncSyncResult sync_client_send_friend_request(const char *base_url, const char *target);
KsyncSyncResult sync_client_get_friend_requests(const char *base_url, char *out, size_t out_size);
KsyncSyncResult sync_client_get_friends(const char *base_url, char *out, size_t out_size);
KsyncSyncResult sync_client_accept_friend_request(const char *base_url, const char *request_id);
KsyncSyncResult sync_client_decline_friend_request(const char *base_url, const char *request_id);
KsyncSyncResult sync_client_remove_friend(const char *base_url, const char *friend_user_id);
KsyncSyncResult sync_client_get_friend_stats(const char *base_url, const char *app,
                                                  const char *practice, const char *metric,
                                                  char *out, size_t out_size);
KsyncSyncResult sync_client_wait_for_remote_event(const char *base_url);
KsyncSyncResult sync_client_delete_account(const char *base_url);
void sync_client_clear_auth_token(void);
#if defined(__EMSCRIPTEN__)
int sync_client_web_start_remote_events(const char *base_url);
int sync_client_web_poll_remote_event(void);
int sync_client_web_sync_start(const char *base_url);
int sync_client_web_sync_poll(KsyncSyncResult *result, int *changed);
#endif

#endif
