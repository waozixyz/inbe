#include "theme.h"
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static LotusThemeScope scopes[LOTUS_THEME_MAX_SCOPES];
static int scope_count = 0;

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

bool theme_parse_color(const char *text, Color *color)
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

const char *theme_color_text(Color color, char *buffer, int size)
{
    if(buffer == NULL || size <= 0)
        return "";

    snprintf(buffer, (size_t)size, "#%02X%02X%02X%02X", color.r, color.g, color.b, color.a);
    return buffer;
}

void theme_reset(void)
{
    memset(scopes, 0, sizeof(scopes));
    scope_count = 0;
}

LotusThemeScope *theme_scope(const char *name)
{
    if(name == NULL)
        return NULL;

    for(int i = 0; i < scope_count; i++) {
        if(strcmp(scopes[i].name, name) == 0)
            return &scopes[i];
    }
    return NULL;
}

const LotusThemeScope *theme_scope_at(int index)
{
    if(index < 0 || index >= scope_count)
        return NULL;
    return &scopes[index];
}

int theme_scope_count(void)
{
    return scope_count;
}

static LotusThemeValue *scope_value(LotusThemeScope *scope, const char *key)
{
    if(scope == NULL || key == NULL)
        return NULL;

    for(int i = 0; i < scope->count; i++) {
        if(strcmp(scope->values[i].key, key) == 0)
            return &scope->values[i];
    }
    return NULL;
}

static LotusThemeValue *scope_add_value(LotusThemeScope *scope, const char *key, Color color)
{
    LotusThemeValue *value;

    if(scope == NULL || key == NULL || key[0] == '\0')
        return NULL;

    value = scope_value(scope, key);
    if(value != NULL) {
        value->value = color;
        return value;
    }

    if(scope->count >= LOTUS_THEME_MAX_VALUES)
        return NULL;

    value = &scope->values[scope->count++];
    memset(value, 0, sizeof(*value));
    copy_text(value->key, LOTUS_THEME_NAME_SIZE, key);
    value->value = color;
    return value;
}

static void load_scope_file(LotusThemeScope *scope)
{
    FILE *file;
    char line[256];

    if(scope == NULL || scope->path[0] == '\0')
        return;

    file = fopen(scope->path, "r");
    if(file == NULL)
        return;

    while(fgets(line, sizeof(line), file) != NULL) {
        char key[LOTUS_THEME_NAME_SIZE];
        char value[32];
        char *cursor = line;
        int key_len = 0;
        Color color;

        while(isspace((unsigned char)*cursor))
            cursor++;
        if(*cursor == '#' || *cursor == '\0')
            continue;

        while(*cursor != '\0' && !isspace((unsigned char)*cursor) &&
              key_len < LOTUS_THEME_NAME_SIZE - 1) {
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

        if(theme_parse_color(value, &color))
            scope_add_value(scope, key, color);
    }

    fclose(file);
}

LotusThemeScope *theme_register_scope(const char *name, const char *path)
{
    LotusThemeScope *scope = theme_scope(name);
    if(scope == NULL) {
        if(scope_count >= LOTUS_THEME_MAX_SCOPES)
            return NULL;
        scope = &scopes[scope_count++];
    }

    memset(scope, 0, sizeof(*scope));
    copy_text(scope->name, LOTUS_THEME_NAME_SIZE, name);
    copy_text(scope->path, LOTUS_THEME_PATH_SIZE, path);
    load_scope_file(scope);
    return scope;
}

Color theme_get(const char *scope_name, const char *key)
{
    LotusThemeValue *value = scope_value(theme_scope(scope_name), key);
    if(value != NULL)
        return value->value;

    fprintf(stderr, "missing theme color: %s.%s, using clean light sky default\n",
            scope_name != NULL ? scope_name : "(null)",
            key != NULL ? key : "(null)");

    // Use clean light sky theme colors as defaults
    if(key != NULL) {
        if(strstr(key, "background") != NULL)
            return (Color){0xE2, 0xEE, 0xFC, 0xFF}; // #E2EEFCFF
        if(strstr(key, "text") != NULL || strstr(key, "foreground") != NULL)
            return (Color){0x24, 0x48, 0x7C, 0xFF}; // #24487CFF
        if(strstr(key, "circle") != NULL)
            return (Color){0x7E, 0xB7, 0xE6, 0xFF}; // #7EB7E6FF
        if(strstr(key, "button_hover") != NULL)
            return (Color){0x68, 0x9E, 0xD7, 0xFF}; // #689ED7FF
        if(strstr(key, "button") != NULL)
            return (Color){0xA6, 0xCF, 0xF2, 0xFF}; // #A6CFF2FF
        if(strstr(key, "icon") != NULL)
            return (Color){0xE2, 0xEE, 0xFC, 0xFF}; // #E2EEFCFF
        if(strstr(key, "link") != NULL)
            return (Color){0x4A, 0x90, 0xE2, 0xFF}; // #4A90E2FF
    }

    // Default fallback: sky background
    return (Color){0xE2, 0xEE, 0xFC, 0xFF};
}

bool theme_set_color(const char *scope_name, const char *key, Color color)
{
    LotusThemeValue *value = scope_value(theme_scope(scope_name), key);
    if(value == NULL)
        return false;
    value->value = color;
    return true;
}

bool theme_save_scope(const char *scope_name)
{
    LotusThemeScope *scope = theme_scope(scope_name);
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

    fprintf(file, "# Lotus theme: %s\n", scope->name);
    for(int i = 0; i < scope->count; i++)
        fprintf(file, "%s \"%s\"\n", scope->values[i].key,
                theme_color_text(scope->values[i].value, text, sizeof(text)));

    fclose(file);
    return true;
}

bool theme_save_all(void)
{
    bool ok = true;
    for(int i = 0; i < scope_count; i++) {
        if(scopes[i].path[0] != '\0' && !theme_save_scope(scopes[i].name))
            ok = false;
    }
    return ok;
}

void theme_draw_tk_border(Rectangle rec, int borderWidth, bool raised)
{
    Color highlight = theme_get("lotus", "border_light");
    Color shadow = theme_get("lotus", "border_shadow");
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
