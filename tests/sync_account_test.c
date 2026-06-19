#include "sync_account.h"
#include "storage.h"

#include <dirent.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int g_failures = 0;
static char g_data_root[1024] = "";

void
data_init(void)
{
}

const char *
data_root(void)
{
    return g_data_root;
}

void
TraceLog(int log_level, const char *text, ...)
{
    (void)log_level;
    (void)text;
}

char *
LoadFileText(const char *file_name)
{
    FILE *file;
    long size;
    char *text;

    file = fopen(file_name, "rb");
    if(file == NULL)
        return NULL;
    if(fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    size = ftell(file);
    if(size < 0) {
        fclose(file);
        return NULL;
    }
    rewind(file);
    text = (char *)malloc((size_t)size + 1);
    if(text == NULL) {
        fclose(file);
        return NULL;
    }
    if(fread(text, 1, (size_t)size, file) != (size_t)size) {
        free(text);
        fclose(file);
        return NULL;
    }
    text[size] = '\0';
    fclose(file);
    return text;
}

void
UnloadFileText(char *text)
{
    free(text);
}

bool
SaveFileData(const char *file_name, const void *data, int data_size)
{
    FILE *file;
    int ok;

    file = fopen(file_name, "wb");
    if(file == NULL)
        return 0;
    ok = fwrite(data, 1, (size_t)data_size, file) == (size_t)data_size;
    fclose(file);
    return ok;
}

static void
check_true(const char *label, int ok)
{
    if(ok)
        return;
    fprintf(stderr, "FAIL %s\n", label);
    g_failures++;
}

static void
check_false(const char *label, int ok)
{
    if(!ok)
        return;
    fprintf(stderr, "FAIL %s\n", label);
    g_failures++;
}

static void
check_str(const char *label, const char *got, const char *want)
{
    if(got != NULL && want != NULL && strcmp(got, want) == 0)
        return;
    fprintf(stderr, "FAIL %s: got %s, want %s\n",
            label, got != NULL ? got : "(null)", want != NULL ? want : "(null)");
    g_failures++;
}

static int
ensure_dir(const char *path)
{
    struct stat st;
    if(stat(path, &st) == 0 && S_ISDIR(st.st_mode))
        return 1;
    return mkdir(path, 0700) == 0;
}

static void
remove_tree(const char *path)
{
    DIR *dir = opendir(path);
    struct dirent *entry;

    if(dir == NULL) {
        remove(path);
        return;
    }
    while((entry = readdir(dir)) != NULL) {
        char child[1024];
        if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
        remove_tree(child);
    }
    closedir(dir);
    rmdir(path);
}

static void
make_clean_root(char *out, size_t out_size, const char *name)
{
    snprintf(out, out_size, "/tmp/inbe-sync-account-test-%ld-%s", (long)getpid(), name);
    remove_tree(out);
    check_true("create test root", ensure_dir(out));
    snprintf(g_data_root, sizeof(g_data_root), "%s", out);
}

static void
bytes_to_hex_local(const unsigned char *bytes, size_t len, char *out, size_t out_size)
{
    static const char hex[] = "0123456789abcdef";

    check_true("hex output size", out_size >= len * 2 + 1);
    for(size_t i = 0; i < len; i++) {
        out[i * 2] = hex[bytes[i] >> 4];
        out[i * 2 + 1] = hex[bytes[i] & 0x0f];
    }
    out[len * 2] = '\0';
}

static void
write_key_file(const char *path, const char *public_id,
               const char *public_key, const char *private_key)
{
    FILE *file = fopen(path, "wb");

    check_true("open key file", file != NULL);
    if(file == NULL)
        return;
    fprintf(file,
            "inbe-sync-key-v1\nalgorithm=ML-DSA-44\npublic_id=%s\npublic_key=%s\nprivate_key=%s\n",
            public_id, public_key, private_key);
    fclose(file);
}

static void
make_account_values(char public_id[65], char public_key_hex[2625],
                    char private_key_hex[5121])
{
    unsigned char public_key[1312];
    unsigned char private_key[2560];

    for(size_t i = 0; i < sizeof(public_key); i++)
        public_key[i] = (unsigned char)(i * 31U + 7U);
    for(size_t i = 0; i < sizeof(private_key); i++)
        private_key[i] = (unsigned char)(i * 17U + 3U);

    inbe_sync_sha256_hex(public_key, sizeof(public_key), public_id);
    bytes_to_hex_local(public_key, sizeof(public_key), public_key_hex, 2625);
    bytes_to_hex_local(private_key, sizeof(private_key), private_key_hex, 5121);
}

static void
test_import_export_clear(void)
{
    char root[512];
    char key_path[1024];
    char export_path[1024];
    char public_id[65];
    char public_key[2625];
    char private_key[5121];
    InbeSyncAccount account;
    char *exported;

    make_clean_root(root, sizeof(root), "roundtrip");
    snprintf(key_path, sizeof(key_path), "%s/inbe-sync.key", root);
    snprintf(export_path, sizeof(export_path), "%s/exported.key", root);
    make_account_values(public_id, public_key, private_key);
    write_key_file(key_path, public_id, public_key, private_key);

    check_true("init storage", inbe_storage_init(root));
    check_true("import key", inbe_sync_account_import_private_key(&account, key_path));
    check_str("import public id", account.public_id, public_id);
    check_true("load imported key", inbe_sync_account_load(&account));
    check_str("load public key", account.public_key_hex, public_key);
    check_str("load private key", account.private_key_hex, private_key);

    check_true("export key", inbe_sync_account_export_private_key(&account, export_path));
    exported = LoadFileText(export_path);
    check_true("read exported key", exported != NULL);
    if(exported != NULL) {
        check_true("export includes public key", strstr(exported, "\npublic_key=") != NULL);
        UnloadFileText(exported);
    }

    check_true("clear key", inbe_sync_account_clear());
    check_false("load after clear", inbe_sync_account_load(&account));
    check_true("import exported key", inbe_sync_account_import_private_key(&account, export_path));
    check_str("reimport public id", account.public_id, public_id);

    inbe_storage_close();
    remove_tree(root);
}

static void
test_reject_invalid_keys(void)
{
    char root[512];
    char key_path[1024];
    char public_id[65];
    char public_key[2625];
    char private_key[5121];
    InbeSyncAccount account;
    FILE *file;

    make_clean_root(root, sizeof(root), "invalid");
    snprintf(key_path, sizeof(key_path), "%s/inbe-sync.key", root);
    make_account_values(public_id, public_key, private_key);

    check_true("init invalid storage", inbe_storage_init(root));

    public_id[0] = public_id[0] == '0' ? '1' : '0';
    write_key_file(key_path, public_id, public_key, private_key);
    check_false("reject mismatched public id",
                inbe_sync_account_import_private_key(&account, key_path));

    file = fopen(key_path, "wb");
    check_true("open missing public key file", file != NULL);
    if(file != NULL) {
        fprintf(file,
                "inbe-sync-key-v1\nalgorithm=ML-DSA-44\npublic_id=%s\nprivate_key=%s\n",
                public_id, private_key);
        fclose(file);
    }
    check_false("reject missing public key",
                inbe_sync_account_import_private_key(&account, key_path));
    check_false("no account after rejected imports", inbe_sync_account_load(&account));

    inbe_storage_close();
    remove_tree(root);
}

int
main(void)
{
    test_import_export_clear();
    test_reject_invalid_keys();

    if(g_failures != 0) {
        fprintf(stderr, "%d sync account test failure(s)\n", g_failures);
        return 1;
    }
    printf("sync account tests passed\n");
    return 0;
}
