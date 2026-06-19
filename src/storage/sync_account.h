#ifndef INBE_SYNC_ACCOUNT_H
#define INBE_SYNC_ACCOUNT_H

#include <stddef.h>

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

#endif
