#include "theme.h"

static int g_current_theme_id = FLINT_THEME_SKY;
static int g_current_dark_mode = 0;

void theme_reset(void) { flint_theme_reset(); }
LotusThemeScope *theme_register_scope(const char *name, const char *path) { return flint_theme_register_scope(name, path); }
LotusThemeScope *theme_scope(const char *name) { return flint_theme_scope(name); }
const LotusThemeScope *theme_scope_at(int index) { return flint_theme_scope_at(index); }
int theme_scope_count(void) { return flint_theme_scope_count(); }
Color theme_get(const char *scope, const char *key) { return flint_theme_get(scope, key); }
bool theme_set_color(const char *scope, const char *key, Color color) { return flint_theme_set_color(scope, key, color); }
bool theme_save_scope(const char *scope) { return flint_theme_save_scope(scope); }
bool theme_save_all(void) { return flint_theme_save_all(); }
const char *theme_color_text(Color color, char *buffer, int size) { return flint_theme_color_text(color, buffer, size); }
bool theme_parse_color(const char *text, Color *color) { return flint_theme_parse_color(text, color); }
void theme_draw_tk_border(Rectangle rec, int borderWidth, bool raised) { flint_theme_draw_tk_border(rec, borderWidth, raised); }

void theme_set_current(int theme_id, int dark_mode)
{
    if(theme_id >= 0 && theme_id < FLINT_THEME_COUNT)
        g_current_theme_id = theme_id;
    else
        g_current_theme_id = FLINT_THEME_SKY;
    g_current_dark_mode = dark_mode != 0;
}

static Color theme_get_color(const char *key)
{
    Color color;
    FlintThemeId theme = flint_theme_normalize(g_current_theme_id);
    bool dark = g_current_dark_mode != 0;
    flint_theme_catalog_color(theme, dark, key, &color);
    return color;
}

Color theme_get_text(void) { return theme_get_color("text"); }
Color theme_get_bg(void) { return theme_get_color("background"); }
Color theme_get_surface(void) { return theme_get_color("surface"); }
Color theme_get_circle(void) { return theme_get_color("circle"); }
Color theme_get_button(void) { return theme_get_color("button"); }
Color theme_get_button_hover(void) { return theme_get_color("button_hover"); }
Color theme_get_icon(void) { return theme_get_color("icon"); }
