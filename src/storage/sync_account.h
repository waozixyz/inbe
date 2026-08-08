#ifndef INBE_SYNC_ACCOUNT_H
#define INBE_SYNC_ACCOUNT_H

#include <stddef.h>
#include <stdint.h>

#include "ksync_account.h"

typedef enum InbeSyncAccountSaveResult {
    INBE_SYNC_ACCOUNT_SAVE_FAILED = 0,
    INBE_SYNC_ACCOUNT_SAVE_OK = 1,
    INBE_SYNC_ACCOUNT_SAVE_NEEDS_CLEAR = 2
} InbeSyncAccountSaveResult;

int sync_account_available(void);
int sync_account_load(KsyncAccount *account);
int sync_account_generate(KsyncAccount *account);
int sync_account_import_private_key_preview(KsyncAccount *account, const char *filename);
InbeSyncAccountSaveResult sync_account_save(KsyncAccount *account, int clear_local_data);
int sync_account_clear(void);
int sync_account_export_private_key(const KsyncAccount *account, const char *filename);
void sync_sha256_hex(const uint8_t *data, size_t len, char out_hex[65]);
int sync_account_sign_hex(const uint8_t *message, size_t message_len,
                               char *out_signature_hex, size_t out_size);

#endif
