#include "flint_locale.h"
#include "platform.h"
#include "flint_embedded_assets.h"

#include "raylib.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FlintLocaleEntry *g_base_entries = NULL;
static size_t g_base_count = 0;
static size_t g_base_cap = 0;

static FlintLocaleEntry *g_active_entries = NULL;
static size_t g_active_count = 0;
static size_t g_active_cap = 0;

static FlintLocaleLanguage *g_languages = NULL;
static size_t g_language_count = 0;
static size_t g_language_cap = 0;

static int g_loaded = 0;
static char g_current_code[32] = "en";

static char *
dup_range(const char *src, size_t len)
{
    char *out = malloc(len + 1);
    if(out == NULL)
        return NULL;
    memcpy(out, src, len);
    out[len] = '\0';
    return out;
}

static char *
dup_cstr(const char *src)
{
    return dup_range(src, strlen(src));
}

static void
free_locale_entries(FlintLocaleEntry **entries, size_t *count, size_t *cap)
{
    if(entries == NULL || *entries == NULL)
        return;

    for(size_t i = 0; i < *count; i++) {
        free((*entries)[i].key);
        free((*entries)[i].value);
    }
    free(*entries);
    *entries = NULL;
    *count = 0;
    *cap = 0;
}

static void
free_languages(void)
{
    if(g_languages == NULL)
        return;

    for(size_t i = 0; i < g_language_count; i++) {
        free(g_languages[i].code);
        free(g_languages[i].label);
    }
    free(g_languages);
    g_languages = NULL;
    g_language_count = 0;
    g_language_cap = 0;
}

static void
ensure_locale_capacity(FlintLocaleEntry **entries, size_t *cap, size_t count)
{
    FlintLocaleEntry *next;
    size_t next_cap;

    if(count < *cap)
        return;

    next_cap = (*cap == 0) ? 32 : *cap * 2;
    next = realloc(*entries, next_cap * sizeof(*next));
    if(next == NULL)
        return;

    *entries = next;
    *cap = next_cap;
}

static void
ensure_language_capacity(void)
{
    FlintLocaleLanguage *next;
    size_t next_cap;

    if(g_language_count < g_language_cap)
        return;

    next_cap = (g_language_cap == 0) ? 8 : g_language_cap * 2;
    next = realloc(g_languages, next_cap * sizeof(*next));
    if(next == NULL)
        return;

    g_languages = next;
    g_language_cap = next_cap;
}

static void
set_locale_entry(FlintLocaleEntry **entries, size_t *count, size_t *cap, const char *key, const char *value)
{
    size_t i;

    if(entries == NULL || count == NULL || cap == NULL || key == NULL || value == NULL)
        return;

    for(i = 0; i < *count; i++) {
        if(strcmp((*entries)[i].key, key) == 0) {
            char *new_value = dup_cstr(value);
            if(new_value == NULL)
                return;
            free((*entries)[i].value);
            (*entries)[i].value = new_value;
            return;
        }
    }

    ensure_locale_capacity(entries, cap, *count);
    if(*count >= *cap)
        return;

    (*entries)[*count].key = dup_cstr(key);
    (*entries)[*count].value = dup_cstr(value);
    if((*entries)[*count].key == NULL || (*entries)[*count].value == NULL) {
        free((*entries)[*count].key);
        free((*entries)[*count].value);
        (*entries)[*count].key = NULL;
        (*entries)[*count].value = NULL;
        return;
    }
    (*count)++;
}

static const char *
find_locale_entry(FlintLocaleEntry *entries, size_t count, const char *key)
{
    if(key == NULL)
        return "";

    for(size_t i = 0; i < count; i++) {
        if(strcmp(entries[i].key, key) == 0)
            return entries[i].value;
    }
    return NULL;
}

static void
append_text(char **buf, size_t *len, size_t *cap, const char *text)
{
    size_t text_len;
    char *next;
    size_t needed;

    if(text == NULL)
        return;

    text_len = strlen(text);
    needed = *len + text_len + 1;
    if(*cap < needed) {
        size_t next_cap = (*cap == 0) ? 128 : *cap;
        while(next_cap < needed)
            next_cap *= 2;
        next = realloc(*buf, next_cap);
        if(next == NULL)
            return;
        *buf = next;
        *cap = next_cap;
    }

    memcpy(*buf + *len, text, text_len);
    *len += text_len;
    (*buf)[*len] = '\0';
}

static void
append_char(char **buf, size_t *len, size_t *cap, char c)
{
    char tmp[2] = { c, '\0' };
    append_text(buf, len, cap, tmp);
}

static void
load_locale_text_into(FlintLocaleEntry **entries, size_t *count, size_t *cap, const char *text)
{
    char *mutable_text;
    char *cursor;
    char *current_key = NULL;
    char *body = NULL;
    size_t body_len = 0;
    size_t body_cap = 0;

    if(text == NULL || entries == NULL || count == NULL || cap == NULL)
        return;

    mutable_text = dup_cstr(text);
    if(mutable_text == NULL)
        return;

    cursor = mutable_text;
    while(cursor != NULL && *cursor != '\0') {
        char *line = cursor;
        char *eol = strpbrk(cursor, "\r\n");
        if(eol != NULL) {
            char line_end = *eol;
            *eol = '\0';
            cursor = eol + 1;
            if((line_end == '\r' && *cursor == '\n') || (line_end == '\n' && *cursor == '\r'))
                cursor++;
        } else {
            cursor = NULL;
        }

        if(line[0] == '[') {
            size_t line_len = strlen(line);
            if(line_len > 2 && line[line_len - 1] == ']') {
                char *key = dup_range(line + 1, line_len - 2);
                if(key != NULL) {
                    free(current_key);
                    current_key = key;
                    free(body);
                    body = NULL;
                    body_len = 0;
                    body_cap = 0;
                }
                continue;
            }
        }

        if(strcmp(line, "---") == 0) {
            if(current_key != NULL) {
                set_locale_entry(entries, count, cap, current_key, body != NULL ? body : "");
                free(current_key);
                current_key = NULL;
            }
            free(body);
            body = NULL;
            body_len = 0;
            body_cap = 0;
            continue;
        }

        if(current_key == NULL)
            continue;

        if(body_len > 0)
            append_char(&body, &body_len, &body_cap, '\n');
        append_text(&body, &body_len, &body_cap, line);
    }

    if(current_key != NULL)
        set_locale_entry(entries, count, cap, current_key, body != NULL ? body : "");

    free(current_key);
    free(body);
    free(mutable_text);
}

static void
load_language_list_from_text(const char *text)
{
    char *mutable_text;
    char *cursor;

    if(text == NULL)
        return;

    mutable_text = dup_cstr(text);
    if(mutable_text == NULL)
        return;

    cursor = mutable_text;
    while(cursor != NULL && *cursor != '\0') {
        char *line = cursor;
        char *eol = strpbrk(cursor, "\r\n");
        if(eol != NULL) {
            char line_end = *eol;
            *eol = '\0';
            cursor = eol + 1;
            if((line_end == '\r' && *cursor == '\n') || (line_end == '\n' && *cursor == '\r'))
                cursor++;
        } else {
            cursor = NULL;
        }

        if(line[0] == '\0' || line[0] == '#')
            continue;

        char *sep = strchr(line, '|');
        if(sep == NULL)
            continue;

        *sep = '\0';
        const char *code = line;
        const char *label = sep + 1;
        if(code[0] == '\0' || label[0] == '\0')
            continue;

        ensure_language_capacity();
        if(g_language_count >= g_language_cap)
            continue;

        g_languages[g_language_count].code = dup_cstr(code);
        g_languages[g_language_count].label = dup_cstr(label);
        if(g_languages[g_language_count].code == NULL || g_languages[g_language_count].label == NULL) {
            free(g_languages[g_language_count].code);
            free(g_languages[g_language_count].label);
            g_languages[g_language_count].code = NULL;
            g_languages[g_language_count].label = NULL;
            continue;
        }
        g_language_count++;
    }

    free(mutable_text);
}

static int
load_file_text_from_paths(const char *relative_path, char **out_text)
{
    char *text;

    if(out_text == NULL)
        return 0;
    *out_text = NULL;

    text = flint_embedded_asset_text(relative_path);
    if(text != NULL) {
        *out_text = text;
        return 1;
    }

#if defined(FLINT_EMBEDDED_ONLY)
    return 0;
#elif INBE_ANDROID_BUILD
    text = LoadFileText(relative_path);
    if(text != NULL) {
        *out_text = text;
        return 1;
    }

    return 0;
#else
    static const char *prefixes[] = {
        "",
        "../",
        "../../",
        "../../../",
        "../../../../",
        NULL
    };

    char path[256];

    if(out_text == NULL)
        return 0;
    *out_text = NULL;

    for(int i = 0; prefixes[i] != NULL; i++) {
        snprintf(path, sizeof(path), "%s%s", prefixes[i], relative_path);
        if(FileExists(path)) {
            char *text = LoadFileText(path);
            if(text != NULL) {
                *out_text = text;
                return 1;
            }
        }
    }
    return 0;
#endif
}

static int
load_locale_file_for_code(const char *code, char **out_text)
{
    char path[64];
    char *text;

    if(out_text == NULL)
        return 0;
    *out_text = NULL;

    if(code == NULL || code[0] == '\0')
        code = "en";

    snprintf(path, sizeof(path), "locales/%s.txt", code);
    text = flint_embedded_asset_text(path);
    if(text != NULL) {
        *out_text = text;
        return 1;
    }

#if defined(FLINT_EMBEDDED_ONLY)
    return 0;
#elif INBE_ANDROID_BUILD
    text = LoadFileText(path);
    if(text != NULL) {
        *out_text = text;
        return 1;
    }

    return 0;
#else
    static const char *prefixes[] = {
        "",
        "../",
        "../../",
        "../../../",
        "../../../../",
        NULL
    };
    for(int i = 0; prefixes[i] != NULL; i++) {
        snprintf(path, sizeof(path), "%slocales/%s.txt", prefixes[i], code);
        if(FileExists(path)) {
            char *text = LoadFileText(path);
            if(text != NULL) {
                *out_text = text;
                return 1;
            }
        }
    }

    return 0;
#endif
}

static void
load_defaults(void)
{
    int has_english = 0;

    for(size_t i = 0; i < g_language_count; i++) {
        if(g_languages[i].code != NULL && strcmp(g_languages[i].code, "en") == 0) {
            has_english = 1;
            break;
        }
    }

    if(has_english)
        return;

    ensure_language_capacity();
    if(g_language_count < g_language_cap) {
        g_languages[g_language_count].code = dup_cstr("en");
        g_languages[g_language_count].label = dup_cstr("English");
        if(g_languages[g_language_count].code != NULL && g_languages[g_language_count].label != NULL)
            g_language_count++;
        else {
            free(g_languages[g_language_count].code);
            free(g_languages[g_language_count].label);
            g_languages[g_language_count].code = NULL;
            g_languages[g_language_count].label = NULL;
        }
    }
}

static void
set_current_code(const char *code)
{
    if(code == NULL || code[0] == '\0')
        code = "en";
    snprintf(g_current_code, sizeof(g_current_code), "%s", code);
}

static void
load_base_locale(void)
{
    char *text = NULL;
    if(load_file_text_from_paths("locales/en.txt", &text)) {
        load_locale_text_into(&g_base_entries, &g_base_count, &g_base_cap, text);
        UnloadFileText(text);
    }
}

static void
load_registry(void)
{
    char *text = NULL;
    if(load_file_text_from_paths("locales/index.txt", &text)) {
        load_language_list_from_text(text);
        UnloadFileText(text);
    }
    load_defaults();
}

static void
clear_active_locale(void)
{
    free_locale_entries(&g_active_entries, &g_active_count, &g_active_cap);
}

void
locale_init(void)
{
    if(g_loaded)
        return;

    free_locale_entries(&g_base_entries, &g_base_count, &g_base_cap);
    free_locale_entries(&g_active_entries, &g_active_count, &g_active_cap);
    free_languages();
    load_registry();
    load_base_locale();
    clear_active_locale();
    set_current_code("en");
    g_loaded = 1;
}

int
locale_set(const char *code)
{
    char *text = NULL;
    int found = 0;

    if(!g_loaded)
        locale_init();

    if(code == NULL || code[0] == '\0')
        code = "en";

    if(strcmp(code, "en") == 0) {
        clear_active_locale();
        set_current_code("en");
        return 1;
    }

    for(size_t i = 0; i < g_language_count; i++) {
        if(strcmp(g_languages[i].code, code) == 0) {
            found = 1;
            break;
        }
    }
    if(!found)
        return 0;

    clear_active_locale();
    if(load_locale_file_for_code(code, &text)) {
        load_locale_text_into(&g_active_entries, &g_active_count, &g_active_cap, text);
        UnloadFileText(text);
        set_current_code(code);
        return 1;
    }

    clear_active_locale();
    set_current_code("en");
    return 0;
}

static const char *
lookup_locale_value(const char *key)
{
    const char *value;

    value = find_locale_entry(g_active_entries, g_active_count, key);
    if(value != NULL)
        return value;

    value = find_locale_entry(g_base_entries, g_base_count, key);
    if(value != NULL)
        return value;

    return key != NULL ? key : "";
}

const char *
locale_get(const char *key)
{
    if(!g_loaded)
        locale_init();
    return lookup_locale_value(key);
}

void
locale_format(char *dst, size_t dst_size, const char *key, ...)
{
    const char *fmt;
    va_list args;

    if(dst == NULL || dst_size == 0)
        return;

    fmt = locale_get(key);
    va_start(args, key);
    vsnprintf(dst, dst_size, fmt, args);
    va_end(args);
}

int
locale_count(void)
{
    if(!g_loaded)
        locale_init();
    return (int)g_language_count;
}

const char *
locale_code_at(int index)
{
    if(!g_loaded)
        locale_init();
    if(index < 0 || (size_t)index >= g_language_count)
        return "";
    return g_languages[index].code != NULL ? g_languages[index].code : "";
}

const char *
locale_label_at(int index)
{
    if(!g_loaded)
        locale_init();
    if(index < 0 || (size_t)index >= g_language_count)
        return "";
    return g_languages[index].label != NULL ? g_languages[index].label : "";
}

int
locale_index_of(const char *code)
{
    if(!g_loaded)
        locale_init();
    if(code == NULL)
        return -1;
    for(size_t i = 0; i < g_language_count; i++) {
        if(g_languages[i].code != NULL && strcmp(g_languages[i].code, code) == 0)
            return (int)i;
    }
    return -1;
}

const char *
locale_current_code(void)
{
    if(!g_loaded)
        locale_init();
    return g_current_code;
}

int
locale_current_index(void)
{
    return locale_index_of(locale_current_code());
}
