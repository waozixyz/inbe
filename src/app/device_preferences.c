#include "device_preferences.h"
#include "theme.h"
#include "flint_ui.h"
#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
#include "android_device.h"
#endif
#if defined(PLATFORM_WEB)
#include "flint_web.h"
#endif

#include <stdbool.h>

void
refresh_theme_colors(int theme_id, int dark_mode)
{
    theme_set_current(theme_id, dark_mode);
    ui_set_colors(theme_get_text(), theme_get_bg(), theme_get_surface(),
                  theme_get_circle(), theme_get_button(), theme_get_button_hover(),
                  theme_get_icon());
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
