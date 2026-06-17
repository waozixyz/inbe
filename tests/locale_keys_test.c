#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    MAX_KEYS = 1024,
    MAX_KEY_LEN = 128,
    MAX_PATH_LEN = 512
};

typedef struct LocaleKeys {
    char keys[MAX_KEYS][MAX_KEY_LEN];
    int count;
} LocaleKeys;

static int failures = 0;

static void
trim_newline(char *text)
{
    size_t len;

    if(text == NULL)
        return;
    len = strlen(text);
    while(len > 0 && (text[len - 1] == '\n' || text[len - 1] == '\r')) {
        text[len - 1] = '\0';
        len--;
    }
}

static int
parse_key_line(const char *line, char *out, size_t out_size)
{
    const char *end;
    size_t len;

    if(line == NULL || out == NULL || out_size == 0 || line[0] != '[')
        return 0;
    end = strchr(line, ']');
    if(end == NULL || end == line + 1)
        return 0;
    len = (size_t)(end - line - 1);
    if(len >= out_size)
        len = out_size - 1;
    memcpy(out, line + 1, len);
    out[len] = '\0';
    return 1;
}

static int
keys_contains(const LocaleKeys *keys, const char *key)
{
    if(keys == NULL || key == NULL)
        return 0;
    for(int i = 0; i < keys->count; i++) {
        if(strcmp(keys->keys[i], key) == 0)
            return 1;
    }
    return 0;
}

static void
keys_add(LocaleKeys *keys, const char *key)
{
    if(keys == NULL || key == NULL || key[0] == '\0')
        return;
    if(keys_contains(keys, key))
        return;
    if(keys->count >= MAX_KEYS) {
        fprintf(stderr, "FAIL too many locale keys\n");
        failures++;
        return;
    }
    snprintf(keys->keys[keys->count], sizeof(keys->keys[keys->count]), "%s", key);
    keys->count++;
}

static int
line_has_value(const char *line)
{
    if(line == NULL)
        return 0;
    while(*line == ' ' || *line == '\t')
        line++;
    return line[0] != '\0' && strcmp(line, "---") != 0;
}

static int
load_locale_keys(const char *path, LocaleKeys *keys)
{
    FILE *fp;
    char line[512];
    char pending_key[MAX_KEY_LEN] = "";
    int ok = 1;

    if(keys == NULL)
        return 0;
    memset(keys, 0, sizeof(*keys));
    fp = fopen(path, "rb");
    if(fp == NULL) {
        fprintf(stderr, "FAIL open %s\n", path);
        failures++;
        return 0;
    }

    while(fgets(line, sizeof(line), fp) != NULL) {
        char key[MAX_KEY_LEN];
        trim_newline(line);
        if(parse_key_line(line, key, sizeof(key))) {
            if(pending_key[0] != '\0') {
                fprintf(stderr, "FAIL %s key [%s] has no value\n", path, pending_key);
                failures++;
                ok = 0;
            }
            keys_add(keys, key);
            snprintf(pending_key, sizeof(pending_key), "%s", key);
        } else if(pending_key[0] != '\0' && line_has_value(line)) {
            pending_key[0] = '\0';
        }
    }
    if(pending_key[0] != '\0') {
        fprintf(stderr, "FAIL %s key [%s] has no value\n", path, pending_key);
        failures++;
        ok = 0;
    }
    fclose(fp);
    return ok;
}

static void
check_locale_file(const LocaleKeys *english, const char *path)
{
    LocaleKeys translated;

    load_locale_keys(path, &translated);
    for(int i = 0; i < english->count; i++) {
        if(!keys_contains(&translated, english->keys[i])) {
            fprintf(stderr, "FAIL %s missing [%s]\n", path, english->keys[i]);
            failures++;
        }
    }
}

int
main(void)
{
    LocaleKeys english;
    DIR *dir;
    struct dirent *entry;

    if(!load_locale_keys("locales/en.txt", &english))
        return 1;

    dir = opendir("locales");
    if(dir == NULL) {
        fprintf(stderr, "FAIL open locales directory\n");
        return 1;
    }
    while((entry = readdir(dir)) != NULL) {
        char path[MAX_PATH_LEN];
        const char *name = entry->d_name;
        size_t len = strlen(name);

        if(len <= 4 || strcmp(name + len - 4, ".txt") != 0)
            continue;
        if(strcmp(name, "en.txt") == 0 || strcmp(name, "index.txt") == 0)
            continue;
        snprintf(path, sizeof(path), "locales/%s", name);
        check_locale_file(&english, path);
    }
    closedir(dir);

    if(failures != 0) {
        fprintf(stderr, "%d locale key test failure(s)\n", failures);
        return 1;
    }
    printf("locale key tests passed\n");
    return 0;
}
