#ifndef INBE_STORAGE_JSON_BUILDER_H
#define INBE_STORAGE_JSON_BUILDER_H

#include <stddef.h>

typedef struct StorageJsonBuilder {
    char *data;
    size_t len;
    size_t cap;
    int ok;
} StorageJsonBuilder;

int storage_json_builder_reserve(StorageJsonBuilder *json, size_t extra);
void storage_json_builder_append_len(StorageJsonBuilder *json, const char *text,
                                     size_t len);
void storage_json_builder_append(StorageJsonBuilder *json, const char *text);
void storage_json_builder_appendf(StorageJsonBuilder *json, const char *fmt, ...);
void storage_json_builder_append_string(StorageJsonBuilder *json, const char *text);
void storage_json_builder_append_key_string(StorageJsonBuilder *json, const char *key,
                                            const char *value);
void storage_json_builder_free(StorageJsonBuilder *json);
void storage_json_builder_append_epoch(StorageJsonBuilder *json, long long seconds);

#endif
