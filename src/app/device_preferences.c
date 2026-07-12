#include "device_preferences.h"
#include "platform.h"
#include "theme.h"
#include "ui.h"
#if ANDROID_BUILD
#include "android_device.h"
#endif
#if defined(PLATFORM_WEB)
#include "web.h"
#endif

#include <stdbool.h>

void
app_set_host_api(InbeApp *app, InbeHostApi host)
{
    if(app == NULL)
        return;
    app->host = host;
}

static bool
app_request_orientation_size(InbeApp *app)
{
    int width = 0;
    int height = 0;

    if(app == NULL || app->host.request_size == NULL)
        return false;

    if(app->host.get_size != NULL)
        app->host.get_size(app->host.userdata, &width, &height);
    else {
        width = config.width;
        height = config.height;
    }
    if(width <= 0 || height <= 0)
        return true;

    if(app->orientation_mode == APP_ORIENTATION_PORTRAIT && width > height)
        app->host.request_size(app->host.userdata, height, width);
    else if(app->orientation_mode == APP_ORIENTATION_LANDSCAPE && height > width)
        app->host.request_size(app->host.userdata, height, width);

    return true;
}

void
refresh_theme_colors(int theme_id, int dark_mode)
{
    SetThemeMode(dark_mode ? THEME_MODE_DARK : THEME_MODE_LIGHT);
    SetCurrentTheme(theme_id, dark_mode);
    SetUIColors(GetThemeText(), GetThemeBackground(), GetThemeSurface(),
                  GetThemeCircle(), GetThemeButton(), GetThemeButtonHover(),
                  GetThemeIcon());
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
    SetSystemThemeDarkMode(android_device_system_dark() != 0);
    return android_device_system_dark() != 0;
#else
    return GetEffectiveThemeDarkMode();
#endif
}

void
app_refresh_theme(InbeApp *app)
{
    if(app == NULL)
        return;
    SetThemeSource(app->theme_source == APP_THEME_SOURCE_SYSTEM
                               ? THEME_SOURCE_SYSTEM
                               : THEME_SOURCE_APP);
#if !ANDROID_BUILD
    if(app->theme_source == APP_THEME_SOURCE_SYSTEM)
        app->theme_mode = APP_THEME_SYSTEM;
#endif
    SetThemeMode((ThemeMode)app->theme_mode);
#if ANDROID_BUILD
    SetSystemThemeDarkMode(android_device_system_dark() != 0);
#endif
    app->dark_mode = app_effective_dark_mode(app);
#if ANDROID_BUILD
    if(app->theme_source == APP_THEME_SOURCE_SYSTEM)
        SetSystemThemeDarkMode(app->dark_mode != 0);
#endif
    SetCurrentTheme(app->theme_id, app->dark_mode);
    SetUIColors(GetThemeText(), GetThemeBackground(), GetThemeSurface(),
                  GetThemeCircle(), GetThemeButton(), GetThemeButtonHover(),
                  GetThemeIcon());
}

void
app_apply_orientation_preference(InbeApp *app)
{
    if(app == NULL)
        return;
    if(app_request_orientation_size(app))
        return;
#if ANDROID_BUILD
    android_device_set_orientation_mode(app->orientation_mode);
#elif defined(PLATFORM_WEB)
    SetWebOrientationMode(app->orientation_mode);
    SyncWebWindowSize();
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
    RefreshSystemTheme();
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
    SetSystemThemeDarkMode(android_device_system_dark() != 0);
#endif
    if(app->android_orientation != orientation)
        app->android_orientation = orientation;

    effective_dark = app_effective_dark_mode(app);
    if(app->dark_mode != effective_dark) {
        app->dark_mode = effective_dark;
        app_refresh_theme(app);
    }
}
