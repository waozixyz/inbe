/* Canvas-build shim: the ksync account crypto (liboqs) and HTTP transport
 * are not carried in this target. Every entry point reports
 * unavailable/failure, so the app degrades to local-only mode. */
#include "ksync_account.h"
#include "ksync_sync.h"

#include <stddef.h>
#include <stdlib.h>

int IsKsyncAccountAvailable(void) { return 0; }

int HasKsyncAccountValues(const KsyncAccount *account)
{
    return account != NULL && account->public_id[0] != '\0' &&
           account->public_key_hex[0] != '\0' &&
           account->private_key_hex[0] != '\0';
}

int CreateKsyncAccount(KsyncAccount *account)
{
    (void)account;
    return 0;
}

int ValidateKsyncAccount(KsyncAccount *account)
{
    (void)account;
    return 0;
}

int ParseKsyncAccountText(const char *text, KsyncAccount *account)
{
    (void)text;
    (void)account;
    return 0;
}

int ExportKsyncAccountText(const KsyncAccount *account, char *out,
                           size_t out_size)
{
    (void)account;
    (void)out;
    (void)out_size;
    return 0;
}

int ImportKsyncAccountFile(const char *filename, KsyncAccount *account)
{
    (void)filename;
    (void)account;
    return 0;
}

int ExportKsyncAccountFile(const KsyncAccount *account, const char *filename)
{
    (void)account;
    (void)filename;
    return 0;
}

int ExportKsyncAccountTextEncrypted(const KsyncAccount *account,
                                    const char *passphrase, char *out,
                                    size_t out_size)
{
    (void)account;
    (void)passphrase;
    (void)out;
    (void)out_size;
    return 0;
}

int ExportKsyncAccountFileEncrypted(const KsyncAccount *account,
                                    const char *passphrase,
                                    const char *filename)
{
    (void)account;
    (void)passphrase;
    (void)filename;
    return 0;
}

int ParseKsyncAccountTextEncrypted(const char *text, const char *passphrase,
                                   KsyncAccount *account)
{
    (void)text;
    (void)passphrase;
    (void)account;
    return 0;
}

int ImportKsyncAccountFileEncrypted(const char *filename,
                                    const char *passphrase,
                                    KsyncAccount *account)
{
    (void)filename;
    (void)passphrase;
    (void)account;
    return 0;
}

void KsyncSha256Hex(const uint8_t *data, size_t len,
                    char out_hex[KSYNC_PUBLIC_ID_HEX_SIZE])
{
    (void)data;
    (void)len;
    if(out_hex != NULL)
        out_hex[0] = '\0';
}

int SignKsyncAccountHex(const KsyncAccount *account, const uint8_t *message,
                        size_t message_len, char *out_signature_hex,
                        size_t out_size)
{
    (void)account;
    (void)message;
    (void)message_len;
    (void)out_signature_hex;
    (void)out_size;
    return 0;
}

const char *GetKsyncSyncResultName(KsyncSyncResult result)
{
    (void)result;
    return "unavailable";
}

int IsKsyncSyncURLValid(const char *url)
{
    (void)url;
    return 0;
}

int NormalizeKsyncSyncURL(const char *input, char *out, size_t out_size)
{
    (void)input;
    (void)out;
    (void)out_size;
    return 0;
}

int JoinKsyncSyncURL(char *out, size_t out_size, const char *base_url,
                     const char *path)
{
    (void)out;
    (void)out_size;
    (void)base_url;
    (void)path;
    return 0;
}

int JoinKsyncSyncWebSocketURL(char *out, size_t out_size,
                              const char *base_url, const char *path)
{
    (void)out;
    (void)out_size;
    (void)base_url;
    (void)path;
    return 0;
}

int AppendKsyncSyncBuffer(KsyncSyncBuffer *buffer, const void *data,
                          size_t bytes)
{
    (void)buffer;
    (void)data;
    (void)bytes;
    return 0;
}

int AppendKsyncSyncBufferJSONString(KsyncSyncBuffer *buffer,
                                    const char *text)
{
    (void)buffer;
    (void)text;
    return 0;
}

void FreeKsyncSyncBuffer(KsyncSyncBuffer *buffer)
{
    if(buffer != NULL) {
        free(buffer->data);
        buffer->data = NULL;
        buffer->len = 0;
        buffer->cap = 0;
    }
}

int FindKsyncSyncJSONString(const char *json, const char *key,
                            char *out, size_t out_size)
{
    (void)json;
    (void)key;
    (void)out;
    (void)out_size;
    return 0;
}

long long FindKsyncSyncJSONInt64(const char *json, const char *key,
                                 long long fallback)
{
    (void)json;
    (void)key;
    return fallback;
}

void ClearKsyncSyncAuthToken(const KsyncSyncConfig *cfg)
{
    (void)cfg;
}

KsyncSyncResult LoginKsyncSync(const KsyncSyncConfig *cfg)
{
    (void)cfg;
    return KSYNC_SYNC_NO_ACCOUNT;
}

KsyncSyncResult RunKsyncSync(const KsyncSyncConfig *cfg)
{
    (void)cfg;
    return KSYNC_SYNC_NO_ACCOUNT;
}

KsyncSyncResult RequestKsyncSyncBearer(const KsyncSyncConfig *cfg,
                                       const char *method, const char *path,
                                       const char *body, char *out,
                                       size_t out_size)
{
    (void)cfg;
    (void)method;
    (void)path;
    (void)body;
    (void)out;
    (void)out_size;
    return KSYNC_SYNC_NO_ACCOUNT;
}

KsyncSyncResult DeleteKsyncSyncAccount(const KsyncSyncConfig *cfg)
{
    (void)cfg;
    return KSYNC_SYNC_NO_ACCOUNT;
}

int WrapKsyncSyncPayload(const KsyncAccount *account, const char *payload,
                         char **out)
{
    (void)account;
    (void)payload;
    (void)out;
    return 0;
}

int UnwrapKsyncSyncPayload(const KsyncAccount *account,
                           const char *envelope_json, char **out)
{
    (void)account;
    (void)envelope_json;
    (void)out;
    return 0;
}

int KsyncDefaultHttpRequest(const char *method, const char *url,
                            const char *body, const char *const *headers,
                            int header_count, KsyncSyncBuffer *response,
                            long *status, void *user)
{
    (void)method;
    (void)url;
    (void)body;
    (void)headers;
    (void)header_count;
    (void)response;
    (void)status;
    (void)user;
    return 0;
}

KsyncSyncResult KsyncRemoteEventWait(const KsyncSyncConfig *cfg,
                                     const char *path)
{
    (void)cfg;
    (void)path;
    return KSYNC_SYNC_NO_ACCOUNT;
}

int KsyncWebSyncStart(const KsyncSyncConfig *cfg)
{
    (void)cfg;
    return 0;
}

int KsyncWebSyncPoll(KsyncSyncResult *result, int *changed)
{
    (void)result;
    (void)changed;
    return 0;
}

int KsyncWebRemoteEventsStart(const KsyncSyncConfig *cfg, const char *path)
{
    (void)cfg;
    (void)path;
    return 0;
}

int KsyncWebRemoteEventsPoll(void)
{
    return 0;
}
