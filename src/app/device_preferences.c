#include "device_preferences.h"
#include "platform.h"
#include "flint_theme.h"
#include "flint_ui.h"
#if ANDROID_BUILD
#include "android_device.h"
#endif
#if defined(PLATFORM_WEB)
#include "flint_web.h"
#endif

#include <stdbool.h>

void
refresh_theme_colors(int theme_id, int dark_mode)
{
    flint_theme_set_mode(dark_mode ? FLINT_THEME_MODE_DARK : FLINT_THEME_MODE_LIGHT);
    flint_theme_set_current(theme_id, dark_mode);
    ui_set_colors(flint_theme_get_text(), flint_theme_get_bg(), flint_theme_get_surface(),
                  flint_theme_get_circle(), flint_theme_get_button(), flint_theme_get_button_hover(),
                  flint_theme_get_icon());
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
#if ANDROID_BUILD
    flint_theme_set_system_dark_mode(android_device_system_dark() != 0);
    return android_device_system_dark() != 0;
#else
    return flint_theme_effective_dark_mode();
#endif
}

void
app_refresh_theme(InbeApp *app)
{
    if(app == NULL)
        return;
    flint_theme_set_source(app->theme_source == APP_THEME_SOURCE_SYSTEM
                               ? FLINT_THEME_SOURCE_SYSTEM
                               : FLINT_THEME_SOURCE_APP);
#if !ANDROID_BUILD
    if(app->theme_source == APP_THEME_SOURCE_SYSTEM)
        app->theme_mode = APP_THEME_SYSTEM;
#endif
    flint_theme_set_mode((FlintThemeMode)app->theme_mode);
#if ANDROID_BUILD
    flint_theme_set_system_dark_mode(android_device_system_dark() != 0);
#endif
    app->dark_mode = app_effective_dark_mode(app);
#if ANDROID_BUILD
    if(app->theme_source == APP_THEME_SOURCE_SYSTEM)
        flint_theme_set_system_dark_mode(app->dark_mode != 0);
#endif
    flint_theme_set_current(app->theme_id, app->dark_mode);
    ui_set_colors(flint_theme_get_text(), flint_theme_get_bg(), flint_theme_get_surface(),
                  flint_theme_get_circle(), flint_theme_get_button(), flint_theme_get_button_hover(),
                  flint_theme_get_icon());
}

void
app_apply_orientation_preference(InbeApp *app)
{
    if(app == NULL)
        return;
#if ANDROID_BUILD
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
#if ANDROID_BUILD
    app->android_orientation = android_device_orientation();
#else
    app->android_orientation = APP_DEVICE_ORIENTATION_UNKNOWN;
#if !defined(PLATFORM_WEB)
    flint_theme_refresh_system();
#endif
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

#if ANDROID_BUILD
    orientation = android_device_orientation();
    flint_theme_set_system_dark_mode(android_device_system_dark() != 0);
#endif
    if(app->android_orientation != orientation)
        app->android_orientation = orientation;

    effective_dark = app_effective_dark_mode(app);
    if(app->dark_mode != effective_dark) {
        app->dark_mode = effective_dark;
        app_refresh_theme(app);
    }
}
