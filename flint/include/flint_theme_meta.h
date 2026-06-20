#ifndef FLINT_THEME_META_H
#define FLINT_THEME_META_H

#include "raylib.h"

#include <stdbool.h>

#define FLINT_THEME_COUNT 12

typedef enum {
    FLINT_THEME_SKY = 0,
    FLINT_THEME_OCEAN = 1,
    FLINT_THEME_FOREST = 2,
    FLINT_THEME_SUNSET = 3,
    FLINT_THEME_LAVENDER = 4,
    FLINT_THEME_CHERRY = 5,
    FLINT_THEME_DAWN = 6,
    FLINT_THEME_SAGE = 7,
    FLINT_THEME_INK = 8,
    FLINT_THEME_MONO = 9,
    FLINT_THEME_MINT = 10,
    FLINT_THEME_COBALT = 11
} FlintThemeId;

typedef struct {
    const char *name;
    const char *light_scope;
    const char *dark_scope;
} FlintThemeMeta;

extern const FlintThemeMeta flint_themes[FLINT_THEME_COUNT];

const FlintThemeMeta *flint_theme_meta(FlintThemeId theme);
FlintThemeId flint_theme_normalize(int theme);
const char *flint_theme_label(FlintThemeId theme);
const char *flint_theme_scope_for(FlintThemeId theme, bool dark_mode);
bool flint_theme_catalog_color(FlintThemeId theme, bool dark_mode, const char *key, Color *color);
bool flint_theme_catalog_scope_color(const char *scope, const char *key, Color *color);

#endif
