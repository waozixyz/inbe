#ifndef THEME_META_H
#define THEME_META_H

#define THEME_COUNT 5

typedef enum {
    ThemeSky = 0,
    ThemeOcean = 1,
    ThemeForest = 2,
    ThemeSunset = 3,
    ThemeLavender = 4
} ThemeId;

typedef struct {
    const char *name;
    const char *light_scope;
    const char *dark_scope;
} ThemeMeta;

extern const ThemeMeta g_themes[THEME_COUNT];

#endif
