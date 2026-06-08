#ifndef THEME_H
#define THEME_H

#include "flint.h"

#define LOTUS_THEME_MAX_SCOPES FLINT_THEME_MAX_SCOPES
#define LOTUS_THEME_MAX_VALUES FLINT_THEME_MAX_VALUES
#define LOTUS_THEME_NAME_SIZE FLINT_THEME_NAME_SIZE
#define LOTUS_THEME_PATH_SIZE FLINT_THEME_PATH_SIZE

typedef FlintThemeValue LotusThemeValue;
typedef FlintThemeScope LotusThemeScope;

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
