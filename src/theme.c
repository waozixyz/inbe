#include "theme.h"

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
