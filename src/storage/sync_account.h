#ifndef INBE_SYNC_ACCOUNT_H
#define INBE_SYNC_ACCOUNT_H

#include <stddef.h>
#include <stdint.h>

#include "lyra_account.h"

enum {
    INBE_SYNC_PUBLIC_ID_HEX_SIZE = LYRA_PUBLIC_ID_HEX_SIZE,
    INBE_SYNC_PUBLIC_KEY_HEX_SIZE = LYRA_PUBLIC_KEY_HEX_SIZE,
    INBE_SYNC_PRIVATE_KEY_HEX_SIZE = LYRA_PRIVATE_KEY_HEX_SIZE
};

typedef LyraAccount InbeSyncAccount;

int sync_account_available(void);
int sync_account_load(InbeSyncAccount *account);
int sync_account_create(InbeSyncAccount *account);
int sync_account_import_private_key(InbeSyncAccount *account, const char *filename);
int sync_account_clear(void);
int sync_account_export_private_key(const InbeSyncAccount *account, const char *filename);
void sync_sha256_hex(const uint8_t *data, size_t len, char out_hex[65]);
int sync_account_sign_hex(const uint8_t *message, size_t message_len,
                               char *out_signature_hex, size_t out_size);

#endif
