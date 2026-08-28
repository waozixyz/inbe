#ifndef INBE_SYNC_CLIENT_H
#define INBE_SYNC_CLIENT_H

#include "ksync_sync.h"

#include <stddef.h>

int sync_client_url_valid(const char *url);
int sync_client_normalize_url(const char *input, char *out, size_t out_size);
int sync_client_normalize_friend_target(const char *target, char *out, size_t out_size);
KsyncSyncResult sync_client_sync(const char *base_url);
KsyncSyncResult sync_client_connect(const char *base_url);
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
