#include "flint_theme.h"
#include "flint_embedded_assets.h"
#include "flint_theme_meta.h"
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static FlintThemeScope scopes[FLINT_THEME_MAX_SCOPES];
static int scope_count = 0;
static bool dark_mode = false;
static int current_theme_id = FLINT_THEME_SKY;

static FlintThemeAggregateVariable aggregate_vars[FLINT_THEME_MAX_VARS];
static int aggregate_count = 0;

static void copy_text(char *dst, int size, const char *src)
{
    snprintf(dst, (size_t)size, "%s", src ? src : "");
}

static int hex_value(char c)
{
    if(c >= '0' && c <= '9')
        return c - '0';
    if(c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if(c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

static unsigned char hex_byte(const char *text)
{
    int hi = hex_value(text[0]);
    int lo = hex_value(text[1]);
    if(hi < 0 || lo < 0)
        return 0;
    return (unsigned char)(hi * 16 + lo);
}

bool flint_theme_parse_color(const char *text, Color *color)
{
    int len;

    if(text == NULL || color == NULL)
        return false;
    if(text[0] == '#')
        text++;

    len = (int)strlen(text);
    if(len != 6 && len != 8)
        return false;

    for(int i = 0; i < len; i++) {
        if(hex_value(text[i]) < 0)
            return false;
    }

    color->r = hex_byte(text);
    color->g = hex_byte(text + 2);
    color->b = hex_byte(text + 4);
    color->a = (len == 8) ? hex_byte(text + 6) : 255;
    return true;
}

const char *flint_theme_color_text(Color color, char *buffer, int size)
{
    if(buffer == NULL || size <= 0)
        return "";

    snprintf(buffer, (size_t)size, "#%02X%02X%02X%02X", color.r, color.g, color.b, color.a);
    return buffer;
}

void flint_theme_reset(void)
{
    memset(scopes, 0, sizeof(scopes));
    scope_count = 0;
}

FlintThemeScope *flint_theme_scope(const char *name)
{
    if(name == NULL)
        return NULL;

    for(int i = 0; i < scope_count; i++) {
        if(strcmp(scopes[i].name, name) == 0)
            return &scopes[i];
    }
    return NULL;
}

const FlintThemeScope *flint_theme_scope_at(int index)
{
    if(index < 0 || index >= scope_count)
        return NULL;
    return &scopes[index];
}

int flint_theme_scope_count(void)
{
    return scope_count;
}

static FlintThemeValue *scope_value(FlintThemeScope *scope, const char *key)
{
    if(scope == NULL || key == NULL)
        return NULL;

    for(int i = 0; i < scope->count; i++) {
        if(strcmp(scope->values[i].key, key) == 0)
            return &scope->values[i];
    }
    return NULL;
}

static FlintThemeValue *scope_add_value(FlintThemeScope *scope, const char *key, Color color)
{
    FlintThemeValue *value;

    if(scope == NULL || key == NULL || key[0] == '\0')
        return NULL;

    value = scope_value(scope, key);
    if(value != NULL) {
        value->value = color;
        return value;
    }

    if(scope->count >= FLINT_THEME_MAX_VALUES)
        return NULL;

    value = &scope->values[scope->count++];
    memset(value, 0, sizeof(*value));
    copy_text(value->key, FLINT_THEME_NAME_SIZE, key);
    value->value = color;
    return value;
}

static void load_scope_file(FlintThemeScope *scope)
{
    FILE *file;
    char line[256];
    char *embedded_text = NULL;
    char *embedded_cursor = NULL;

    if(scope == NULL || scope->path[0] == '\0')
        return;

    embedded_text = flint_embedded_asset_text(scope->path);
    if(embedded_text != NULL)
        embedded_cursor = embedded_text;

#if !defined(FLINT_EMBEDDED_ONLY)
    file = fopen(scope->path, "r");
    if(file == NULL && embedded_text == NULL) {
        fprintf(stderr, "warning: could not load theme file '%s' for scope '%s'\n",
                scope->path, scope->name);
        return;
    }
#else
    file = NULL;
    if(embedded_text == NULL)
        return;
#endif

    while(embedded_text != NULL || fgets(line, sizeof(line), file) != NULL) {
        char key[FLINT_THEME_NAME_SIZE];
        char value[32];
        char *cursor = line;
        int key_len = 0;
        Color color;

        if(embedded_text != NULL) {
            char *end = strpbrk(embedded_cursor, "\r\n");
            size_t len;

            if(*embedded_cursor == '\0')
                break;

            len = end != NULL ? (size_t)(end - embedded_cursor) : strlen(embedded_cursor);
            if(len >= sizeof(line))
                len = sizeof(line) - 1;
            memcpy(line, embedded_cursor, len);
            line[len] = '\0';

            if(end != NULL) {
                embedded_cursor = end + 1;
                if((*end == '\r' && *embedded_cursor == '\n') ||
                   (*end == '\n' && *embedded_cursor == '\r'))
                    embedded_cursor++;
            } else {
                embedded_cursor += strlen(embedded_cursor);
            }
        }

        while(isspace((unsigned char)*cursor))
            cursor++;
        if(*cursor == '#' || *cursor == '\0')
            continue;

        while(*cursor != '\0' && !isspace((unsigned char)*cursor) &&
              key_len < FLINT_THEME_NAME_SIZE - 1) {
            key[key_len++] = *cursor++;
        }
        key[key_len] = '\0';

        while(isspace((unsigned char)*cursor))
            cursor++;
        if(*cursor == '"')
            cursor++;

        int value_len = 0;
        while(*cursor != '\0' && *cursor != '"' && *cursor != '\n' &&
              !isspace((unsigned char)*cursor) && value_len < (int)sizeof(value) - 1) {
            value[value_len++] = *cursor++;
        }
        value[value_len] = '\0';

        if(flint_theme_parse_color(value, &color))
            scope_add_value(scope, key, color);
    }

    if(file != NULL)
        fclose(file);
    free(embedded_text);
}

FlintThemeScope *flint_theme_register_scope(const char *name, const char *path)
{
    FlintThemeScope *scope = flint_theme_scope(name);
    if(scope == NULL) {
        if(scope_count >= FLINT_THEME_MAX_SCOPES)
            return NULL;
        scope = &scopes[scope_count++];
    }

    memset(scope, 0, sizeof(*scope));
    copy_text(scope->name, FLINT_THEME_NAME_SIZE, name);
    copy_text(scope->path, FLINT_THEME_PATH_SIZE, path);

    // Auto-generate dark path by inserting "_dark" before extension
    if(path != NULL && path[0] != '\0') {
        char *dot = strrchr((char *)path, '.');
        if(dot != NULL && dot > path) {
            int base_len = (int)(dot - path);
            if(base_len < FLINT_THEME_PATH_SIZE - 10) {
                snprintf(scope->dark_path, FLINT_THEME_PATH_SIZE, "%.*s_dark%s", base_len, path, dot);
            }
        }
    }

    load_scope_file(scope);
    return scope;
}

FlintThemeScope *flint_theme_register_scope_dark(const char *name, const char *path, const char *dark_path)
{
    FlintThemeScope *scope = flint_theme_scope(name);
    if(scope == NULL) {
        if(scope_count >= FLINT_THEME_MAX_SCOPES)
            return NULL;
        scope = &scopes[scope_count++];
    }

    memset(scope, 0, sizeof(*scope));
    copy_text(scope->name, FLINT_THEME_NAME_SIZE, name);
    copy_text(scope->path, FLINT_THEME_PATH_SIZE, path);
    copy_text(scope->dark_path, FLINT_THEME_PATH_SIZE, dark_path);
    load_scope_file(scope);
    return scope;
}

Color flint_theme_get(const char *scope_name, const char *key)
{
    Color catalog_color;
    if(flint_theme_catalog_scope_color(scope_name, key, &catalog_color))
        return catalog_color;

    FlintThemeValue *value = scope_value(flint_theme_scope(scope_name), key);
    if(value != NULL)
        return value->value;

    value = scope_value(flint_theme_scope("flint"), key);
    if(value != NULL)
        return value->value;

    value = scope_value(flint_theme_scope("default"), key);
    if(value != NULL)
        return value->value;

    for(int i = 0; i < scope_count; i++) {
        value = scope_value(&scopes[i], key);
        if(value != NULL)
            return value->value;
    }

    fprintf(stderr, "warning: missing theme color: %s.%s, using fallback\n",
            scope_name != NULL ? scope_name : "(null)",
            key != NULL ? key : "(null)");

    if(key != NULL) {
        if(strstr(key, "background") != NULL || strstr(key, "paper") != NULL)
            return (Color){0xE2, 0xEE, 0xFC, 0xFF};
        if(strstr(key, "surface") != NULL || strstr(key, "modal") != NULL ||
           strstr(key, "panel") != NULL)
            return (Color){0xD4, 0xE4, 0xF5, 0xFF};
        if(strstr(key, "text") != NULL || strstr(key, "foreground") != NULL ||
           strstr(key, "ink") != NULL)
            return (Color){0x24, 0x48, 0x7C, 0xFF};
        if(strstr(key, "circle") != NULL || strstr(key, "selection") != NULL)
            return (Color){0x7E, 0xB7, 0xE6, 0xFF};
        if(strstr(key, "button_hover") != NULL || strstr(key, "face_hover") != NULL)
            return (Color){0x68, 0x9E, 0xD7, 0xFF};
        if(strstr(key, "button") != NULL || strstr(key, "face") != NULL)
            return (Color){0xA6, 0xCF, 0xF2, 0xFF};
        if(strstr(key, "icon") != NULL)
            return (Color){0xE2, 0xEE, 0xFC, 0xFF};
        if(strstr(key, "link") != NULL)
            return (Color){0x4A, 0x90, 0xE2, 0xFF};
    }

    return (Color){0xE2, 0xEE, 0xFC, 0xFF};
}

bool flint_theme_set_color(const char *scope_name, const char *key, Color color)
{
    FlintThemeValue *value = scope_value(flint_theme_scope(scope_name), key);
    if(value == NULL)
        return false;
    value->value = color;
    return true;
}

bool flint_theme_save_scope(const char *scope_name)
{
    FlintThemeScope *scope = flint_theme_scope(scope_name);
    FILE *file;
    char text[16];
    const char *dir;

    if(scope == NULL || scope->path[0] == '\0')
        return false;

    dir = GetDirectoryPath(scope->path);
    if(dir != NULL && dir[0] != '\0' && !DirectoryExists(dir))
        MakeDirectory(dir);

    file = fopen(scope->path, "w");
    if(file == NULL)
        return false;

    fprintf(file, "# Flint theme: %s\n", scope->name);
    for(int i = 0; i < scope->count; i++)
        fprintf(file, "%s \"%s\"\n", scope->values[i].key,
                flint_theme_color_text(scope->values[i].value, text, sizeof(text)));

    fclose(file);
    return true;
}

bool flint_theme_save_all(void)
{
    bool ok = true;
    for(int i = 0; i < scope_count; i++) {
        if(scopes[i].path[0] != '\0' && !flint_theme_save_scope(scopes[i].name))
            ok = false;
    }
    return ok;
}

void flint_theme_draw_tk_border(Rectangle rec, int borderWidth, bool raised)
{
    Color highlight = flint_theme_get(NULL, "border_light");
    Color shadow = flint_theme_get(NULL, "border_shadow");
    Color topLeft = raised ? highlight : shadow;
    Color bottomRight = raised ? shadow : highlight;

    int x = (int)rec.x;
    int y = (int)rec.y;
    int w = (int)rec.width;
    int h = (int)rec.height;

    DrawRectangle(x, y, w, borderWidth, topLeft);
    DrawRectangle(x, y, borderWidth, h, topLeft);
    DrawRectangle(x, y + h - borderWidth, w, borderWidth, bottomRight);
    DrawRectangle(x + w - borderWidth, y, borderWidth, h, bottomRight);
}

static int find_aggregate_var(const char *key)
{
    for(int i = 0; i < aggregate_count; i++) {
        if(strcmp(aggregate_vars[i].key, key) == 0)
            return i;
    }
    return -1;
}

static FlintThemeAggregateVariable *add_aggregate_var(const char *key, Color value, int scope_index)
{
    FlintThemeAggregateVariable *var;

    if(aggregate_count >= FLINT_THEME_MAX_VARS)
        return NULL;

    var = &aggregate_vars[aggregate_count++];
    memset(var, 0, sizeof(*var));
    copy_text(var->key, FLINT_THEME_NAME_SIZE, key);
    var->value = value;
    var->scopes[scope_index] = true;
    var->scope_count = 1;
    return var;
}

void flint_theme_aggregate_all(void)
{
    aggregate_count = 0;
    memset(aggregate_vars, 0, sizeof(aggregate_vars));

    for(int s = 0; s < scope_count; s++) {
        FlintThemeScope *scope = &scopes[s];
        for(int v = 0; v < scope->count; v++) {
            const char *key = scope->values[v].key;
            Color value = scope->values[v].value;
            int idx = find_aggregate_var(key);

            if(idx >= 0) {
                aggregate_vars[idx].scopes[s] = true;
                aggregate_vars[idx].scope_count++;
            } else {
                add_aggregate_var(key, value, s);
            }
        }
    }
}

FlintThemeAggregateVariable* flint_theme_aggregate_vars(void)
{
    return aggregate_vars;
}

int flint_theme_aggregate_count(void)
{
    return aggregate_count;
}

void flint_theme_apply_aggregate(const char *key, Color color)
{
    int idx = find_aggregate_var(key);
    if(idx < 0)
        return;

    aggregate_vars[idx].value = color;

    for(int s = 0; s < scope_count; s++) {
        if(aggregate_vars[idx].scopes[s]) {
            flint_theme_set_color(scopes[s].name, key, color);
        }
    }
}

bool flint_theme_sync_from_scope(const char *src_scope)
{
    FlintThemeScope *source = flint_theme_scope(src_scope);
    if(source == NULL)
        return false;

    for(int s = 0; s < scope_count; s++) {
        if(&scopes[s] == source)
            continue;

        for(int v = 0; v < source->count; v++) {
            const char *key = source->values[v].key;
            Color value = source->values[v].value;
            flint_theme_set_color(scopes[s].name, key, value);
        }
    }
    return true;
}

bool flint_theme_sync_to_apps(const char *src_scope)
{
    return flint_theme_sync_from_scope(src_scope);
}

bool flint_theme_export_theme(const char *path)
{
    FILE *file;
    char text[16];

    if(path == NULL || path[0] == '\0')
        return false;

    file = fopen(path, "w");
    if(file == NULL)
        return false;

    fprintf(file, "# Flint theme export\n");
    fprintf(file, "# Generated by flint_theme_export_theme()\n\n");

    for(int s = 0; s < scope_count; s++) {
        fprintf(file, "# Scope: %s\n", scopes[s].name);
        fprintf(file, "[%s]\n", scopes[s].name);

        for(int v = 0; v < scopes[s].count; v++) {
            fprintf(file, "%s \"%s\"\n", scopes[s].values[v].key,
                    flint_theme_color_text(scopes[s].values[v].value, text, sizeof(text)));
        }
        fprintf(file, "\n");
    }

    fclose(file);
    return true;
}

bool flint_theme_import_theme(const char *path)
{
    FILE *file;
    char line[512];
    char current_scope[FLINT_THEME_NAME_SIZE] = {0};

    if(path == NULL || path[0] == '\0')
        return false;

    file = fopen(path, "r");
    if(file == NULL)
        return false;

    while(fgets(line, sizeof(line), file) != NULL) {
        char *cursor = line;

        while(isspace((unsigned char)*cursor))
            cursor++;

        if(*cursor == '#' || *cursor == '\0')
            continue;

        if(*cursor == '[') {
            char *end = strchr(cursor, ']');
            if(end != NULL) {
                *end = '\0';
                copy_text(current_scope, FLINT_THEME_NAME_SIZE, cursor + 1);
            }
            continue;
        }

        char key[FLINT_THEME_NAME_SIZE];
        char value[32];
        int key_len = 0;
        Color color;

        while(*cursor != '\0' && !isspace((unsigned char)*cursor) &&
              key_len < FLINT_THEME_NAME_SIZE - 1) {
            key[key_len++] = *cursor++;
        }
        key[key_len] = '\0';

        while(isspace((unsigned char)*cursor))
            cursor++;
        if(*cursor == '"')
            cursor++;

        int value_len = 0;
        while(*cursor != '\0' && *cursor != '"' && *cursor != '\n' &&
              !isspace((unsigned char)*cursor) && value_len < (int)sizeof(value) - 1) {
            value[value_len++] = *cursor++;
        }
        value[value_len] = '\0';

        if(flint_theme_parse_color(value, &color)) {
            if(current_scope[0] != '\0') {
                flint_theme_set_color(current_scope, key, color);
            }
        }
    }

    fclose(file);
    return true;
}

void flint_theme_set_dark_mode(bool dark)
{
    dark_mode = dark;
}

bool flint_theme_get_dark_mode(void)
{
    return dark_mode;
}

void flint_theme_reload_themes(void)
{
    for(int i = 0; i < scope_count; i++) {
        FlintThemeScope *scope = &scopes[i];
        const char *load_path = dark_mode && scope->dark_path[0] != '\0' ? scope->dark_path : scope->path;

        // Clear current values
        scope->count = 0;

        // Load from appropriate file
        if(load_path != NULL && load_path[0] != '\0') {
            FILE *file = fopen(load_path, "r");
            if(file != NULL) {
                char line[256];
                while(fgets(line, sizeof(line), file) != NULL) {
                    char key[FLINT_THEME_NAME_SIZE];
                    char value[32];
                    char *cursor = line;
                    int key_len = 0;
                    Color color;

                    while(isspace((unsigned char)*cursor))
                        cursor++;
                    if(*cursor == '#' || *cursor == '\0')
                        continue;

                    while(*cursor != '\0' && !isspace((unsigned char)*cursor) &&
                          key_len < FLINT_THEME_NAME_SIZE - 1) {
                        key[key_len++] = *cursor++;
                    }
                    key[key_len] = '\0';

                    while(isspace((unsigned char)*cursor))
                        cursor++;
                    if(*cursor == '"')
                        cursor++;

                    int value_len = 0;
                    while(*cursor != '\0' && *cursor != '"' && *cursor != '\n' &&
                          !isspace((unsigned char)*cursor) && value_len < (int)sizeof(value) - 1) {
                        value[value_len++] = *cursor++;
                    }
                    value[value_len] = '\0';

                    if(flint_theme_parse_color(value, &color))
                        scope_add_value(scope, key, color);
                }
                fclose(file);
            }
        }
    }

    flint_theme_aggregate_all();
}

void
flint_theme_set_current(int theme_id, int current_dark_mode)
{
    current_theme_id = flint_theme_normalize(theme_id);
    dark_mode = current_dark_mode != 0;
}

Color
flint_theme_current_color(const char *key)
{
    Color color = BLANK;
    flint_theme_catalog_color(flint_theme_normalize(current_theme_id),
                              dark_mode, key, &color);
    return color;
}

Color flint_theme_get_text(void) { return flint_theme_current_color("text"); }
Color flint_theme_get_bg(void) { return flint_theme_current_color("background"); }
Color flint_theme_get_surface(void) { return flint_theme_current_color("surface"); }
Color flint_theme_get_circle(void) { return flint_theme_current_color("circle"); }
Color flint_theme_get_button(void) { return flint_theme_current_color("button"); }
Color flint_theme_get_button_hover(void) { return flint_theme_current_color("button_hover"); }
Color flint_theme_get_icon(void) { return flint_theme_current_color("icon"); }
