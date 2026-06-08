#ifndef THEME_META_H
#define THEME_META_H

#include "flint_theme_meta.h"

#define THEME_COUNT FLINT_THEME_COUNT

typedef enum {
    ThemeSky = FLINT_THEME_SKY,
    ThemeOcean = FLINT_THEME_OCEAN,
    ThemeForest = FLINT_THEME_FOREST,
    ThemeSunset = FLINT_THEME_SUNSET,
    ThemeLavender = FLINT_THEME_LAVENDER,
    ThemeCherry = FLINT_THEME_CHERRY
} ThemeId;

typedef FlintThemeMeta ThemeMeta;

#define g_themes flint_themes

#endif
