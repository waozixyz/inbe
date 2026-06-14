#include "app_preferences.h"
#include "theme.h"
#include "flint_ui.h"
#if defined(LOTUS_BUILD)
#include "lotus_settings.h"
#endif
#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
#include "android_device.h"
#endif
#if defined(PLATFORM_WEB)
#include "flint_web.h"
#endif

#include <stdbool.h>

extern Color c_text, c_bg, c_surface, c_circle, c_button, c_button_hover, c_icon;

void
refresh_theme_colors(int theme_id, int dark_mode)
{
#if defined(LOTUS_BUILD)
    (void)theme_id;
    (void)dark_mode;
    c_bg = lotus_alias_color("background");
    c_text = lotus_alias_color("text");
    c_surface = lotus_alias_color("surface");
    c_circle = lotus_alias_color("circle");
    c_button = lotus_alias_color("button");
    c_button_hover = lotus_alias_color("button_hover");
    c_icon = lotus_alias_color("icon");
    ui_set_colors(c_text, c_bg, c_surface, c_circle, c_button, c_button_hover, c_icon);
    return;
#else
    Color bg;
    Color surface;
    Color text;
    Color circle;
    Color button;
    Color button_hover;
    Color icon;
    FlintThemeId theme;
    bool dark;

    if(theme_id < 0 || theme_id >= FLINT_THEME_COUNT)
        theme_id = FLINT_THEME_SKY;

    theme = flint_theme_normalize(theme_id);
    dark = dark_mode != 0;

    flint_theme_catalog_color(theme, dark, "background", &bg);
    flint_theme_catalog_color(theme, dark, "surface", &surface);
    flint_theme_catalog_color(theme, dark, "text", &text);
    flint_theme_catalog_color(theme, dark, "circle", &circle);
    flint_theme_catalog_color(theme, dark, "button", &button);
    flint_theme_catalog_color(theme, dark, "button_hover", &button_hover);
    flint_theme_catalog_color(theme, dark, "icon", &icon);

    c_bg = bg;
    c_surface = surface;
    c_text = text;
    c_circle = circle;
    c_button = button;
    c_button_hover = button_hover;
    c_icon = icon;

    ui_set_colors(text, bg, surface, circle, button, button_hover, icon);
#endif
}

int
app_effective_dark_mode(InbeApp *app)
{
    if(app == NULL)
        return 0;
    if(app->theme_mode == APP_THEME_LIGHT)
        return 0;
    if(app->theme_mode == APP_THEME_DARK)
        return 1;
#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
    return android_device_system_dark() != 0;
#else
    return 0;
#endif
}

void
app_refresh_theme(InbeApp *app)
{
    if(app == NULL)
        return;
    app->dark_mode = app_effective_dark_mode(app);
    refresh_theme_colors(app->theme_id, app->dark_mode);
}

void
app_apply_orientation_preference(InbeApp *app)
{
    if(app == NULL)
        return;
#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
    android_device_set_orientation_mode(app->orientation_mode);
#elif defined(PLATFORM_WEB)
    flint_web_set_orientation_mode(app->orientation_mode);
    flint_web_sync_window_size();
#else
    if(app->orientation_mode == APP_ORIENTATION_PORTRAIT &&
       GetScreenWidth() > GetScreenHeight()) {
        SetWindowSize(GetScreenHeight(), GetScreenWidth());
    } else if(app->orientation_mode == APP_ORIENTATION_LANDSCAPE &&
              GetScreenHeight() > GetScreenWidth()) {
        SetWindowSize(GetScreenHeight(), GetScreenWidth());
    }
#endif
}

void
app_device_preferences_init(InbeApp *app)
{
    if(app == NULL)
        return;
#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
    app->android_orientation = android_device_orientation();
#else
    app->android_orientation = APP_DEVICE_ORIENTATION_UNKNOWN;
#endif
    app_refresh_theme(app);
    app_apply_orientation_preference(app);
}

void
app_device_preferences_update(InbeApp *app)
{
    int effective_dark;
    int orientation = APP_DEVICE_ORIENTATION_UNKNOWN;

    if(app == NULL)
        return;

#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
    orientation = android_device_orientation();
#endif
    if(app->android_orientation != orientation)
        app->android_orientation = orientation;

    effective_dark = app_effective_dark_mode(app);
    if(app->dark_mode != effective_dark) {
        app->dark_mode = effective_dark;
        refresh_theme_colors(app->theme_id, app->dark_mode);
    }
}
