#include "storage_json_builder.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int
storage_json_builder_reserve(StorageJsonBuilder *json, size_t extra)
{
    char *next;
    size_t next_cap;

    if(json == NULL || !json->ok)
        return 0;
    if(extra <= json->cap - json->len)
        return 1;
    next_cap = json->cap > 0 ? json->cap : 1024;
    while(extra > next_cap - json->len)
        next_cap *= 2;
    next = (char *)realloc(json->data, next_cap);
    if(next == NULL) {
        json->ok = 0;
        return 0;
    }
    json->data = next;
    json->cap = next_cap;
    return 1;
}

void
storage_json_builder_append_len(StorageJsonBuilder *json, const char *text,
                                size_t len)
{
    if(text == NULL || len == 0)
        return;
    if(!storage_json_builder_reserve(json, len + 1))
        return;
    memcpy(json->data + json->len, text, len);
    json->len += len;
    json->data[json->len] = '\0';
}

void
storage_json_builder_append(StorageJsonBuilder *json, const char *text)
{
    if(text != NULL)
        storage_json_builder_append_len(json, text, strlen(text));
}

void
storage_json_builder_appendf(StorageJsonBuilder *json, const char *fmt, ...)
{
    va_list args;
    va_list copy;
    int needed;

    if(json == NULL || fmt == NULL || !json->ok)
        return;
    va_start(args, fmt);
    va_copy(copy, args);
    needed = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if(needed < 0) {
        json->ok = 0;
        va_end(args);
        return;
    }
    if(storage_json_builder_reserve(json, (size_t)needed + 1)) {
        vsnprintf(json->data + json->len, json->cap - json->len, fmt, args);
        json->len += (size_t)needed;
    }
    va_end(args);
}

void
storage_json_builder_append_string(StorageJsonBuilder *json, const char *text)
{
    const unsigned char *p = (const unsigned char *)(text != NULL ? text : "");

    storage_json_builder_append(json, "\"");
    while(*p != '\0') {
        char escaped[8];
        if(*p == '"' || *p == '\\') {
            escaped[0] = '\\';
            escaped[1] = (char)*p;
            storage_json_builder_append_len(json, escaped, 2);
        } else if(*p == '\b') {
            storage_json_builder_append(json, "\\b");
        } else if(*p == '\f') {
            storage_json_builder_append(json, "\\f");
        } else if(*p == '\n') {
            storage_json_builder_append(json, "\\n");
        } else if(*p == '\r') {
            storage_json_builder_append(json, "\\r");
        } else if(*p == '\t') {
            storage_json_builder_append(json, "\\t");
        } else if(*p < 0x20) {
            snprintf(escaped, sizeof(escaped), "\\u%04x", *p);
            storage_json_builder_append(json, escaped);
        } else {
            storage_json_builder_append_len(json, (const char *)p, 1);
        }
        p++;
    }
    storage_json_builder_append(json, "\"");
}

void
storage_json_builder_append_key_string(StorageJsonBuilder *json, const char *key,
                                       const char *value)
{
    storage_json_builder_append_string(json, key);
    storage_json_builder_append(json, ":");
    storage_json_builder_append_string(json, value);
}

void
storage_json_builder_free(StorageJsonBuilder *json)
{
    if(json == NULL)
        return;
    free(json->data);
    memset(json, 0, sizeof(*json));
}

void
storage_json_builder_append_epoch(StorageJsonBuilder *json, long long seconds)
{
    char formatted[32];
    struct tm tm_value;
    time_t value = (time_t)seconds;

    if(seconds <= 0)
        value = time(NULL);
#if defined(_WIN32)
    gmtime_s(&tm_value, &value);
#else
    gmtime_r(&value, &tm_value);
#endif
    strftime(formatted, sizeof(formatted), "%Y-%m-%dT%H:%M:%SZ", &tm_value);
    storage_json_builder_append_string(json, formatted);
}
