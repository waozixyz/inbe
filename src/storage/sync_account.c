#include "sync_account.h"

#include "data.h"
#include "storage.h"
#include "raylib.h"

#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
#include "android_share.h"
#endif

#if defined(INBE_HAS_LIBOQS)
#include <oqs/oqs.h>
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SYNC_PUBLIC_ID_KEY "sync_public_id"
#define SYNC_PUBLIC_KEY_KEY "sync_public_key"
#define SYNC_PRIVATE_KEY_KEY "sync_private_key"

typedef struct Sha256Ctx {
    uint32_t state[8];
    uint64_t bit_len;
    uint8_t data[64];
    size_t data_len;
} Sha256Ctx;

static int account_has_values(const InbeSyncAccount *account);

static const uint32_t sha256_k[64] = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U,
    0x923f82a4U, 0xab1c5ed5U, 0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
    0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U, 0xe49b69c1U, 0xefbe4786U,
    0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U,
    0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
    0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U, 0xa2bfe8a1U, 0xa81a664bU,
    0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU,
    0x5b9cca4fU, 0x682e6ff3U, 0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
};

static uint32_t
rotr32(uint32_t value, uint32_t bits)
{
    return (value >> bits) | (value << (32U - bits));
}

static void
sha256_transform(Sha256Ctx *ctx, const uint8_t data[64])
{
    uint32_t m[64];
    uint32_t a, b, c, d, e, f, g, h;

    for(int i = 0; i < 16; i++) {
        m[i] = ((uint32_t)data[i * 4] << 24) |
               ((uint32_t)data[i * 4 + 1] << 16) |
               ((uint32_t)data[i * 4 + 2] << 8) |
               (uint32_t)data[i * 4 + 3];
    }
    for(int i = 16; i < 64; i++) {
        uint32_t s0 = rotr32(m[i - 15], 7) ^ rotr32(m[i - 15], 18) ^ (m[i - 15] >> 3);
        uint32_t s1 = rotr32(m[i - 2], 17) ^ rotr32(m[i - 2], 19) ^ (m[i - 2] >> 10);
        m[i] = m[i - 16] + s0 + m[i - 7] + s1;
    }

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];

    for(int i = 0; i < 64; i++) {
        uint32_t s1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + s1 + ch + sha256_k[i] + m[i];
        uint32_t s0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = s0 + maj;
        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

static void
sha256_init(Sha256Ctx *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->state[0] = 0x6a09e667U;
    ctx->state[1] = 0xbb67ae85U;
    ctx->state[2] = 0x3c6ef372U;
    ctx->state[3] = 0xa54ff53aU;
    ctx->state[4] = 0x510e527fU;
    ctx->state[5] = 0x9b05688cU;
    ctx->state[6] = 0x1f83d9abU;
    ctx->state[7] = 0x5be0cd19U;
}

static void
sha256_update(Sha256Ctx *ctx, const uint8_t *data, size_t len)
{
    for(size_t i = 0; i < len; i++) {
        ctx->data[ctx->data_len++] = data[i];
        if(ctx->data_len == 64) {
            sha256_transform(ctx, ctx->data);
            ctx->bit_len += 512;
            ctx->data_len = 0;
        }
    }
}

static void
sha256_final(Sha256Ctx *ctx, uint8_t hash[32])
{
    size_t i = ctx->data_len;

    ctx->data[i++] = 0x80;
    if(i > 56) {
        while(i < 64)
            ctx->data[i++] = 0;
        sha256_transform(ctx, ctx->data);
        i = 0;
    }
    while(i < 56)
        ctx->data[i++] = 0;

    ctx->bit_len += ctx->data_len * 8;
    for(int j = 0; j < 8; j++)
        ctx->data[63 - j] = (uint8_t)(ctx->bit_len >> (j * 8));
    sha256_transform(ctx, ctx->data);

    for(i = 0; i < 4; i++) {
        for(int j = 0; j < 8; j++)
            hash[j * 4 + i] = (uint8_t)(ctx->state[j] >> (24 - i * 8));
    }
}

static void
bytes_to_hex(const uint8_t *bytes, size_t len, char *out, size_t out_size)
{
    static const char hex[] = "0123456789abcdef";
    size_t needed = len * 2 + 1;

    if(out == NULL || out_size < needed)
        return;
    for(size_t i = 0; i < len; i++) {
        out[i * 2] = hex[bytes[i] >> 4];
        out[i * 2 + 1] = hex[bytes[i] & 0x0f];
    }
    out[len * 2] = '\0';
}

static int
hex_to_bytes(const char *hex, uint8_t *out, size_t out_len)
{
    size_t len;

    if(hex == NULL || out == NULL)
        return 0;
    len = strlen(hex);
    if(len != out_len * 2)
        return 0;
    for(size_t i = 0; i < out_len; i++) {
        unsigned int value;
        if(sscanf(hex + i * 2, "%2x", &value) != 1)
            return 0;
        out[i] = (uint8_t)value;
    }
    return 1;
}

static int
hex_string_valid(const char *hex, size_t expected_len)
{
    if(hex == NULL || strlen(hex) != expected_len)
        return 0;
    for(size_t i = 0; i < expected_len; i++) {
        char c = hex[i];
        if(!((c >= '0' && c <= '9') ||
             (c >= 'a' && c <= 'f') ||
             (c >= 'A' && c <= 'F')))
            return 0;
    }
    return 1;
}

static void
copy_key_value(char *out, size_t out_size, const char *value, size_t value_len)
{
    if(out == NULL || out_size == 0)
        return;
    if(value_len >= out_size)
        value_len = out_size - 1;
    memcpy(out, value, value_len);
    out[value_len] = '\0';
}

static void
parse_sync_key_line(InbeSyncAccount *account, const char *line, size_t line_len)
{
    const char *value;
    size_t value_len;

    if(account == NULL || line == NULL)
        return;
    if(line_len > 0 && line[line_len - 1] == '\r')
        line_len--;

    if(line_len > 10 && strncmp(line, "public_id=", 10) == 0) {
        value = line + 10;
        value_len = line_len - 10;
        copy_key_value(account->public_id, sizeof(account->public_id), value, value_len);
    } else if(line_len > 11 && strncmp(line, "public_key=", 11) == 0) {
        value = line + 11;
        value_len = line_len - 11;
        copy_key_value(account->public_key_hex, sizeof(account->public_key_hex), value, value_len);
    } else if(line_len > 12 && strncmp(line, "private_key=", 12) == 0) {
        value = line + 12;
        value_len = line_len - 12;
        copy_key_value(account->private_key_hex, sizeof(account->private_key_hex), value, value_len);
    }
}

static int
parse_sync_key_text(const char *text, InbeSyncAccount *account)
{
    const char *line;
    const char *next;

    if(text == NULL || account == NULL)
        return 0;
    memset(account, 0, sizeof(*account));

    line = text;
    while(*line != '\0') {
        next = strchr(line, '\n');
        if(next == NULL) {
            parse_sync_key_line(account, line, strlen(line));
            break;
        }
        parse_sync_key_line(account, line, (size_t)(next - line));
        line = next + 1;
    }
    return account_has_values(account);
}

static int
sync_account_validate_import(InbeSyncAccount *account)
{
    uint8_t public_key[1312];
    char expected_public_id[65];

    if(!account_has_values(account))
        return 0;
    if(!hex_string_valid(account->public_id, 64) ||
       !hex_string_valid(account->public_key_hex, 2624) ||
       !hex_string_valid(account->private_key_hex, 5120))
        return 0;
    if(!hex_to_bytes(account->public_key_hex, public_key, sizeof(public_key)))
        return 0;
    inbe_sync_sha256_hex(public_key, sizeof(public_key), expected_public_id);
    return strcmp(account->public_id, expected_public_id) == 0;
}

void
inbe_sync_sha256_hex(const uint8_t *data, size_t len, char out_hex[65])
{
    uint8_t digest[32];
    Sha256Ctx sha;

    if(out_hex == NULL)
        return;
    out_hex[0] = '\0';
    if(data == NULL && len > 0)
        return;
    sha256_init(&sha);
    sha256_update(&sha, data, len);
    sha256_final(&sha, digest);
    bytes_to_hex(digest, sizeof(digest), out_hex, 65);
}

static int
account_has_values(const InbeSyncAccount *account)
{
    return account != NULL &&
           account->public_id[0] != '\0' &&
           account->public_key_hex[0] != '\0' &&
           account->private_key_hex[0] != '\0';
}

int
inbe_sync_account_available(void)
{
#if defined(INBE_HAS_LIBOQS)
    return 1;
#else
    return 0;
#endif
}

int
inbe_sync_account_load(InbeSyncAccount *account)
{
    const char *public_id;
    const char *public_key;
    const char *private_key;

    data_init();
    if(account == NULL)
        return 0;
    memset(account, 0, sizeof(*account));

    public_id = inbe_storage_get_setting_text(SYNC_PUBLIC_ID_KEY);
    if(public_id != NULL)
        snprintf(account->public_id, sizeof(account->public_id), "%s", public_id);
    public_key = inbe_storage_get_setting_text(SYNC_PUBLIC_KEY_KEY);
    if(public_key != NULL)
        snprintf(account->public_key_hex, sizeof(account->public_key_hex), "%s", public_key);
    private_key = inbe_storage_get_setting_text(SYNC_PRIVATE_KEY_KEY);
    if(private_key != NULL)
        snprintf(account->private_key_hex, sizeof(account->private_key_hex), "%s", private_key);
    return account_has_values(account);
}

int
inbe_sync_account_create(InbeSyncAccount *account)
{
#if defined(INBE_HAS_LIBOQS)
    OQS_SIG *sig;
    uint8_t public_key[1312];
    uint8_t private_key[2560];
    uint8_t digest[32];
    Sha256Ctx sha;
    InbeSyncAccount generated;

    data_init();
    if(account == NULL)
        return 0;
    memset(&generated, 0, sizeof(generated));

    sig = OQS_SIG_new(OQS_SIG_alg_ml_dsa_44);
    if(sig == NULL ||
       sig->length_public_key != sizeof(public_key) ||
       sig->length_secret_key != sizeof(private_key)) {
        if(sig != NULL)
            OQS_SIG_free(sig);
        return 0;
    }
    if(OQS_SIG_keypair(sig, public_key, private_key) != OQS_SUCCESS) {
        OQS_SIG_free(sig);
        return 0;
    }
    OQS_SIG_free(sig);

    sha256_init(&sha);
    sha256_update(&sha, public_key, sizeof(public_key));
    sha256_final(&sha, digest);

    bytes_to_hex(digest, sizeof(digest), generated.public_id, sizeof(generated.public_id));
    bytes_to_hex(public_key, sizeof(public_key), generated.public_key_hex,
                 sizeof(generated.public_key_hex));
    bytes_to_hex(private_key, sizeof(private_key), generated.private_key_hex,
                 sizeof(generated.private_key_hex));

    inbe_storage_settings_begin_write();
    inbe_storage_set_setting_text(SYNC_PUBLIC_ID_KEY, generated.public_id);
    inbe_storage_set_setting_text(SYNC_PUBLIC_KEY_KEY, generated.public_key_hex);
    inbe_storage_set_setting_text(SYNC_PRIVATE_KEY_KEY, generated.private_key_hex);
    inbe_storage_settings_end_write();

    *account = generated;
    return 1;
#else
    (void)account;
    return 0;
#endif
}

int
inbe_sync_account_import_private_key(InbeSyncAccount *account, const char *filename)
{
    char *body;
    InbeSyncAccount imported;

    data_init();
    if(account == NULL || filename == NULL || filename[0] == '\0')
        return 0;

    body = LoadFileText(filename);
    if(body == NULL)
        return 0;

    if(!parse_sync_key_text(body, &imported) ||
       !sync_account_validate_import(&imported)) {
        UnloadFileText(body);
        return 0;
    }
    UnloadFileText(body);

    inbe_storage_settings_begin_write();
    inbe_storage_set_setting_text(SYNC_PUBLIC_ID_KEY, imported.public_id);
    inbe_storage_set_setting_text(SYNC_PUBLIC_KEY_KEY, imported.public_key_hex);
    inbe_storage_set_setting_text(SYNC_PRIVATE_KEY_KEY, imported.private_key_hex);
    inbe_storage_settings_end_write();

    *account = imported;
    return 1;
}

int
inbe_sync_account_clear(void)
{
    data_init();
    inbe_storage_settings_begin_write();
    inbe_storage_set_setting_text(SYNC_PUBLIC_ID_KEY, "");
    inbe_storage_set_setting_text(SYNC_PUBLIC_KEY_KEY, "");
    inbe_storage_set_setting_text(SYNC_PRIVATE_KEY_KEY, "");
    inbe_storage_settings_end_write();
    return 1;
}

int
inbe_sync_account_export_private_key(const InbeSyncAccount *account, const char *filename)
{
    char body[8200];
    int len;

    if(!account_has_values(account) || filename == NULL || filename[0] == '\0')
        return 0;

    len = snprintf(body, sizeof(body),
                   "inbe-sync-key-v1\nalgorithm=ML-DSA-44\npublic_id=%s\npublic_key=%s\nprivate_key=%s\n",
                   account->public_id, account->public_key_hex, account->private_key_hex);
    if(len <= 0 || (size_t)len >= sizeof(body))
        return 0;

#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
    return android_share_bytes((const unsigned char *)body, (size_t)len, filename,
                               "application/octet-stream");
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
inbe_sync_account_sign_hex(const uint8_t *message, size_t message_len,
                           char *out_signature_hex, size_t out_size)
{
#if defined(INBE_HAS_LIBOQS)
    InbeSyncAccount account;
    OQS_SIG *sig;
    uint8_t private_key[2560];
    uint8_t signature[2420];
    size_t signature_len = 0;

    if(out_signature_hex == NULL || out_size < sizeof(signature) * 2 + 1 ||
       (message == NULL && message_len > 0))
        return 0;
    out_signature_hex[0] = '\0';
    if(!inbe_sync_account_load(&account))
        return 0;
    if(!hex_to_bytes(account.private_key_hex, private_key, sizeof(private_key)))
        return 0;

    sig = OQS_SIG_new(OQS_SIG_alg_ml_dsa_44);
    if(sig == NULL ||
       sig->length_secret_key != sizeof(private_key) ||
       sig->length_signature != sizeof(signature)) {
        if(sig != NULL)
            OQS_SIG_free(sig);
        return 0;
    }

    if(OQS_SIG_sign(sig, signature, &signature_len, message, message_len,
                    private_key) != OQS_SUCCESS ||
       signature_len != sizeof(signature)) {
        OQS_SIG_free(sig);
        return 0;
    }
    OQS_SIG_free(sig);

    bytes_to_hex(signature, sizeof(signature), out_signature_hex, out_size);
    return out_signature_hex[0] != '\0';
#else
    (void)message;
    (void)message_len;
    (void)out_signature_hex;
    (void)out_size;
    return 0;
#endif
}
