#ifndef FLINT_THEME_H
#define FLINT_THEME_H

#include "raylib.h"
#include <stdbool.h>

#define FLINT_THEME_MAX_SCOPES 24
#define FLINT_THEME_MAX_VALUES 64
#define FLINT_THEME_NAME_SIZE 64
#define FLINT_THEME_PATH_SIZE 256
#define FLINT_THEME_MAX_VARS 64

typedef struct FlintThemeValue {
    char key[FLINT_THEME_NAME_SIZE];
    Color value;
} FlintThemeValue;

typedef struct FlintThemeScope {
    char name[FLINT_THEME_NAME_SIZE];
    char path[FLINT_THEME_PATH_SIZE];
    char dark_path[FLINT_THEME_PATH_SIZE];
    FlintThemeValue values[FLINT_THEME_MAX_VALUES];
    int count;
} FlintThemeScope;

typedef struct {
    char key[FLINT_THEME_NAME_SIZE];
    Color value;
    bool scopes[FLINT_THEME_MAX_SCOPES];
    int scope_count;
} FlintThemeAggregateVariable;

void flint_theme_reset(void);
FlintThemeScope *flint_theme_register_scope(const char *name, const char *path);
FlintThemeScope *flint_theme_register_scope_dark(const char *name, const char *path, const char *dark_path);
FlintThemeScope *flint_theme_scope(const char *name);
const FlintThemeScope *flint_theme_scope_at(int index);
int flint_theme_scope_count(void);
Color flint_theme_get(const char *scope, const char *key);
bool flint_theme_set_color(const char *scope, const char *key, Color color);
bool flint_theme_save_scope(const char *scope);
bool flint_theme_save_all(void);
const char *flint_theme_color_text(Color color, char *buffer, int size);
bool flint_theme_parse_color(const char *text, Color *color);
void flint_theme_draw_tk_border(Rectangle rec, int borderWidth, bool raised);

void flint_theme_aggregate_all(void);
FlintThemeAggregateVariable* flint_theme_aggregate_vars(void);
int flint_theme_aggregate_count(void);
void flint_theme_apply_aggregate(const char *key, Color color);
bool flint_theme_sync_from_scope(const char *src_scope);
bool flint_theme_sync_to_apps(const char *src_scope);
bool flint_theme_export_theme(const char *path);
bool flint_theme_import_theme(const char *path);

void flint_theme_set_dark_mode(bool dark);
bool flint_theme_get_dark_mode(void);
void flint_theme_reload_themes(void);

void flint_theme_set_current(int theme_id, int dark_mode);
Color flint_theme_current_color(const char *key);
Color flint_theme_get_text(void);
Color flint_theme_get_bg(void);
Color flint_theme_get_surface(void);
Color flint_theme_get_circle(void);
Color flint_theme_get_button(void);
Color flint_theme_get_button_hover(void);
Color flint_theme_get_icon(void);

#endif
