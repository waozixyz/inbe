#ifndef THEME_META_H
#define THEME_META_H

#define THEME_COUNT 6

typedef enum {
    ThemeSky = 0,
    ThemeOcean = 1,
    ThemeForest = 2,
    ThemeSunset = 3,
    ThemeLavender = 4,
    ThemeCherry = 5
} ThemeId;

typedef struct {
    const char *name;
    const char *light_scope;
    const char *dark_scope;
    const char *light_path;
    const char *dark_path;
} ThemeMeta;

extern const ThemeMeta g_themes[THEME_COUNT];

#endif
