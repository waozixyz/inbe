/*
 * settings_keys_test - guards the settings save/load/import key lists
 * against drift. Scans src/app/app_settings.kry and src/storage/import.kry:
 * every save_setting_int / storage_set_setting_text literal forms the saved
 * set; every load_bool_setting / load_clamped_setting / settings_cache_get_int
 * / settings_cache_get literal forms the loaded set; the
 * importable_setting_keys declaration forms the import set. Every saved key
 * must be loaded (modulo derived keys) and every importable key must be
 * saved (modulo keys written through helpers).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_KEYS 512
#define MAX_KEY_LEN 96
#define MAX_FILE (512 * 1024)

typedef struct {
    char keys[MAX_KEYS][MAX_KEY_LEN];
    int count;
} KeySet;

static int failures;

static int
keyset_contains(const KeySet *set, const char *key)
{
    for(int i = 0; i < set->count; i++) {
        if(strcmp(set->keys[i], key) == 0)
            return 1;
    }
    return 0;
}

static void
keyset_add(KeySet *set, const char *key)
{
    if(key == NULL || key[0] == '\0')
        return;
    if(set->count >= MAX_KEYS) {
        fprintf(stderr, "FAIL key set overflow at [%s]\n", key);
        failures++;
        return;
    }
    if(!keyset_contains(set, key))
        snprintf(set->keys[set->count++], MAX_KEY_LEN, "%s", key);
}

/* Keys written through helpers whose literals live outside the direct
 * save_setting_int calls. */
static const char *helper_saved_keys[] = {
    "practice_music_track_wim_hof",
    "practice_music_track_meditation",
    "practice_music_track_sun_salutation",
    NULL
};

/* Keys saved with formatted indices; their load counterparts use the same
 * printf shape, so the literal match below covers them. */
static const char *derived_keys[] = {
    NULL
};

static int
key_is_whitelisted(const char *key, const char **list)
{
    for(int i = 0; list[i] != NULL; i++) {
        if(strcmp(list[i], key) == 0)
            return 1;
    }
    return 0;
}

static char *
read_whole_file(const char *path)
{
    FILE *fp = fopen(path, "rb");
    char *buffer;

    if(fp == NULL)
        return NULL;
    buffer = malloc(MAX_FILE);
    if(buffer == NULL) {
        fclose(fp);
        return NULL;
    }
    buffer[fread(buffer, 1, MAX_FILE - 1, fp)] = '\0';
    fclose(fp);
    return buffer;
}

/* Finds each occurrence of fn( and collects the first double-quoted
 * literal within the following window, spanning newlines. */
static void
scan_first_literal_per_call(KeySet *set, const char *text, const char *fn)
{
    size_t fn_len = strlen(fn);
    const char *cursor = text;

    while((cursor = strstr(cursor, fn)) != NULL) {
        const char *limit = cursor + 2048;
        const char *quote = strchr(cursor + fn_len, '"');

        cursor += fn_len;
        if(quote == NULL || quote > limit)
            continue;
        {
            char key[MAX_KEY_LEN];
            size_t n = 0;
            const char *walk = quote + 1;

            while(*walk != '"' && *walk != '\0' && n + 1 < sizeof(key))
                key[n++] = *walk++;
            key[n] = '\0';
            keyset_add(set, key);
        }
    }
}

/* Collects every double-quoted literal after the marker, up to '}'. */
static void
scan_all_literals_after(KeySet *set, const char *text, const char *marker)
{
    const char *cursor = strstr(text, marker);

    if(cursor == NULL) {
        fprintf(stderr, "FAIL marker [%s] not found\n", marker);
        failures++;
        return;
    }
    const char *end = strchr(cursor, '}');
    for(const char *quote = strchr(cursor, '"'); quote != NULL && (end == NULL || quote < end);
        quote = strchr(quote + 1, '"')) {
        char key[MAX_KEY_LEN];
        size_t n = 0;
        const char *walk = quote + 1;

        while(*walk != '"' && *walk != '\0' && n + 1 < sizeof(key))
            key[n++] = *walk++;
        key[n] = '\0';
        if(*walk != '"')
            break;
        keyset_add(set, key);
        quote = walk;
        if(end != NULL && walk >= end)
            break;
    }
}

int
main(void)
{
    char *settings_src = read_whole_file("src/app/app_settings.kry");
    char *import_src = read_whole_file("src/storage/import.kry");
    KeySet saved, loaded, imported;

    if(settings_src == NULL || import_src == NULL) {
        fprintf(stderr, "FAIL cannot read settings/import sources\n");
        return 1;
    }
    memset(&saved, 0, sizeof(saved));
    memset(&loaded, 0, sizeof(loaded));
    memset(&imported, 0, sizeof(imported));

    scan_first_literal_per_call(&saved, settings_src, "save_setting_int(");
    scan_first_literal_per_call(&saved, settings_src, "storage_set_setting_text(");
    scan_first_literal_per_call(&loaded, settings_src, "load_bool_setting(");
    scan_first_literal_per_call(&loaded, settings_src, "load_clamped_setting(");
    scan_first_literal_per_call(&loaded, settings_src, "settings_cache_get_int(");
    scan_first_literal_per_call(&loaded, settings_src, "settings_cache_get(");
    {
        char *app_src = read_whole_file("src/app/app.c");
        if(app_src == NULL) {
            fprintf(stderr, "FAIL cannot read src/app/app.c\n");
            return 1;
        }
        scan_first_literal_per_call(&loaded, app_src, "storage_get_setting_text(");
        free(app_src);
    }
    scan_all_literals_after(&imported, import_src, "importable_setting_keys");

    for(int i = 0; i < saved.count; i++) {
        if(!keyset_contains(&loaded, saved.keys[i]) &&
           !key_is_whitelisted(saved.keys[i], derived_keys)) {
            fprintf(stderr, "FAIL setting [%s] is saved but never loaded\n",
                    saved.keys[i]);
            failures++;
        }
    }
    for(int i = 0; i < imported.count; i++) {
        if(!keyset_contains(&saved, imported.keys[i]) &&
           !key_is_whitelisted(imported.keys[i], helper_saved_keys)) {
            fprintf(stderr, "FAIL importable setting [%s] is never saved\n",
                    imported.keys[i]);
            failures++;
        }
    }

    printf("saved=%d loaded=%d imported=%d\n", saved.count, loaded.count,
           imported.count);
    free(settings_src);
    free(import_src);
    if(failures != 0) {
        fprintf(stderr, "%d settings key test failure(s)\n", failures);
        return 1;
    }
    printf("settings key tests passed\n");
    return 0;
}
