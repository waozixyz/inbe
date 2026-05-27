#ifndef THEME_H
#define THEME_H

#include "raylib.h"
#include <stdbool.h>

#define LOTUS_THEME_MAX_SCOPES 16
#define LOTUS_THEME_MAX_VALUES 48
#define LOTUS_THEME_NAME_SIZE 64
#define LOTUS_THEME_PATH_SIZE 256

typedef struct LotusThemeValue {
    char key[LOTUS_THEME_NAME_SIZE];
    Color value;
} LotusThemeValue;

typedef struct LotusThemeScope {
    char name[LOTUS_THEME_NAME_SIZE];
    char path[LOTUS_THEME_PATH_SIZE];
    LotusThemeValue values[LOTUS_THEME_MAX_VALUES];
    int count;
} LotusThemeScope;

void theme_reset(void);
LotusThemeScope *theme_register_scope(const char *name, const char *path);
LotusThemeScope *theme_scope(const char *name);
const LotusThemeScope *theme_scope_at(int index);
int theme_scope_count(void);
Color theme_get(const char *scope, const char *key);
bool theme_set_color(const char *scope, const char *key, Color color);
bool theme_save_scope(const char *scope);
bool theme_save_all(void);
const char *theme_color_text(Color color, char *buffer, int size);
bool theme_parse_color(const char *text, Color *color);
void theme_draw_tk_border(Rectangle rec, int borderWidth, bool raised);

#endif
