#ifndef INBE_SYNC_ACCOUNT_H
#define INBE_SYNC_ACCOUNT_H

#include <stddef.h>
#include <stdint.h>

enum {
    INBE_SYNC_PUBLIC_ID_HEX_SIZE = 65,
    INBE_SYNC_PUBLIC_KEY_HEX_SIZE = 2625,
    INBE_SYNC_PRIVATE_KEY_HEX_SIZE = 5121
};

typedef struct InbeSyncAccount {
    char public_id[INBE_SYNC_PUBLIC_ID_HEX_SIZE];
    char public_key_hex[INBE_SYNC_PUBLIC_KEY_HEX_SIZE];
    char private_key_hex[INBE_SYNC_PRIVATE_KEY_HEX_SIZE];
} InbeSyncAccount;

int inbe_sync_account_available(void);
int inbe_sync_account_load(InbeSyncAccount *account);
int inbe_sync_account_create(InbeSyncAccount *account);
int inbe_sync_account_export_private_key(const InbeSyncAccount *account, const char *filename);
void inbe_sync_sha256_hex(const uint8_t *data, size_t len, char out_hex[65]);
int inbe_sync_account_sign_hex(const uint8_t *message, size_t message_len,
                               char *out_signature_hex, size_t out_size);

#endif
