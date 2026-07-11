#include "sync_account.h"
#include "platform.h"

#include "data.h"
#include "storage.h"
#include "sync_client.h"
#include "flint.h"

#if ANDROID_BUILD
#include "android_share.h"
#endif
#if defined(PLATFORM_WEB)
#include <emscripten.h>
#endif

#include <stdio.h>
#include <string.h>

#if !defined(HAS_LIBOQS)
#error "Inbe sync account builds require HAS_LIBOQS; build liboqs for this target instead of disabling sync crypto."
#endif

#define SYNC_PUBLIC_ID_KEY "sync_public_id"
#define SYNC_PUBLIC_KEY_KEY "sync_public_key"
#define SYNC_PRIVATE_KEY_KEY "sync_private_key"
#define SYNC_ACCOUNT_ALIAS_KEY "sync_account_alias"

static int
account_has_values(const InbeSyncAccount *account)
{
    return HasLyraAccountValues(account);
}

static void
sync_account_write_and_reset(const InbeSyncAccount *account)
{
    storage_settings_begin_write();
    storage_set_setting_text(SYNC_PUBLIC_ID_KEY, account != NULL ? account->public_id : "");
    storage_set_setting_text(SYNC_PUBLIC_KEY_KEY, account != NULL ? account->public_key_hex : "");
    storage_set_setting_text(SYNC_PRIVATE_KEY_KEY, account != NULL ? account->private_key_hex : "");
    storage_set_setting_text(SYNC_ACCOUNT_ALIAS_KEY, "");
    sync_client_clear_auth_token();
    storage_reset_sync_state();
    storage_settings_end_write();
}

static int
sync_account_same_owner(const char *owner, const InbeSyncAccount *account)
{
    return owner != NULL && account != NULL &&
           strcmp(owner, account->public_id) == 0;
}

void
sync_sha256_hex(const uint8_t *data, size_t len, char out_hex[65])
{
    LyraSha256Hex(data, len, out_hex);
}

int
sync_account_available(void)
{
    return IsLyraAccountAvailable();
}

int
sync_account_load(InbeSyncAccount *account)
{
    const char *public_id;
    const char *public_key;
    const char *private_key;

    data_init();
    if(account == NULL)
        return 0;
    memset(account, 0, sizeof(*account));

    public_id = storage_get_setting_text(SYNC_PUBLIC_ID_KEY);
    if(public_id != NULL)
        snprintf(account->public_id, sizeof(account->public_id), "%s", public_id);
    public_key = storage_get_setting_text(SYNC_PUBLIC_KEY_KEY);
    if(public_key != NULL)
        snprintf(account->public_key_hex, sizeof(account->public_key_hex), "%s", public_key);
    private_key = storage_get_setting_text(SYNC_PRIVATE_KEY_KEY);
    if(private_key != NULL)
        snprintf(account->private_key_hex, sizeof(account->private_key_hex), "%s", private_key);
    return account_has_values(account);
}

int
sync_account_generate(InbeSyncAccount *account)
{
    data_init();
    if(account == NULL)
        return 0;
    memset(account, 0, sizeof(*account));
    return CreateLyraAccount(account);
}

int
sync_account_import_private_key_preview(InbeSyncAccount *account, const char *filename)
{
    data_init();
    if(account == NULL || filename == NULL || filename[0] == '\0')
        return 0;
    memset(account, 0, sizeof(*account));
    if(!ImportLyraAccountFile(filename, account)) {
        TraceLog(LOG_WARNING, "SYNC: Private key import failed: invalid key file");
        return 0;
    }
    return 1;
}

InbeSyncAccountSaveResult
sync_account_save(InbeSyncAccount *account, int clear_local_data)
{
    const char *owner;

    data_init();
    if(!account_has_values(account))
        return INBE_SYNC_ACCOUNT_SAVE_FAILED;

    owner = storage_sync_data_owner_public_id();
    if(owner != NULL && !sync_account_same_owner(owner, account) &&
       storage_has_local_syncable_data()) {
        if(!clear_local_data)
            return INBE_SYNC_ACCOUNT_SAVE_NEEDS_CLEAR;
        if(!storage_clear_local_syncable_data())
            return INBE_SYNC_ACCOUNT_SAVE_FAILED;
    }

    storage_set_sync_data_owner_public_id(account->public_id);
    sync_account_write_and_reset(account);
    return INBE_SYNC_ACCOUNT_SAVE_OK;
}

int
sync_account_clear(void)
{
    data_init();
    sync_account_write_and_reset(NULL);
    return 1;
}

int
sync_account_export_private_key(const InbeSyncAccount *account, const char *filename)
{
    char body[LYRA_ACCOUNT_EXPORT_TEXT_SIZE];
    int len;

    if(!account_has_values(account) || filename == NULL || filename[0] == '\0')
        return 0;
    if(!ExportLyraAccountText(account, body, sizeof(body)))
        return 0;
    len = (int)strlen(body);

#if ANDROID_BUILD
    return android_share_bytes((const unsigned char *)body, (size_t)len, filename,
                               "application/octet-stream");
#elif defined(PLATFORM_WEB)
    EM_ASM({
        const ptr = $0;
        const len = $1;
        const filename = UTF8ToString($2);
        const bytes = HEAPU8.slice(ptr, ptr + len);
        const blob = new Blob([bytes], {type: "application/octet-stream"});
        const url = URL.createObjectURL(blob);
        const a = document.createElement("a");
        a.href = url;
        a.download = filename || "lyra-account.key";
        a.style.display = "none";
        document.body.appendChild(a);
        a.click();
        a.remove();
        setTimeout(() => URL.revokeObjectURL(url), 1000);
    }, body, len, filename);
    return 1;
#else
    if(strchr(filename, '/') != NULL || strchr(filename, '\\') != NULL)
        return SaveFileData(filename, body, len);
    {
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", data_root(), filename);
        return SaveFileData(path, body, len);
    }
#endif
}

int
sync_account_sign_hex(const uint8_t *message, size_t message_len,
                      char *out_signature_hex, size_t out_size)
{
    InbeSyncAccount account;

    if(!sync_account_load(&account))
        return 0;
    return SignLyraAccountHex(&account, message, message_len, out_signature_hex,
                                       out_size);
}
