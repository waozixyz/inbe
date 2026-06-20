#include "settings_theme.h"

#include "app.h"
#include "app_settings.h"
#include "device_preferences.h"
#include "flint_locale.h"
#include "theme.h"
#include "flint_theme_meta.h"
#include "flint_ui.h"

int
settings_theme_content_height(int content_w)
{
    return flint_px(76) + ui_theme_picker_height(content_w) + flint_px(60);
}

void
settings_theme_draw(InbeApp *app, int x, int w, int *y, SettingsThemeState *state)
{
    const char *theme_mode_options[] = {
        locale_get("theme_system"),
        locale_get("theme_light"),
        locale_get("theme_dark")
    };

    if(app == NULL || y == NULL || state == NULL)
        return;

    app->theme_mode = clampi(app->theme_mode, APP_THEME_SYSTEM, APP_THEME_DARK);
    flint_text_draw(locale_get("theme_mode_label"), x, *y, flint_ui_font(), theme_get_text());
    ui_draw_dropdown_button(102, x, *y + flint_px(26), w, flint_px(36),
                            theme_mode_options, 3, &app->theme_mode);
    state->draw_theme_mode_menu = 1;
    *y += flint_px(76);

    if(ui_draw_theme_picker(x, *y, w, locale_get("theme_label"),
                            app->dark_mode, &app->theme_id)) {
        app->theme_id = clampi(app->theme_id, 0, FLINT_THEME_COUNT - 1);
        app_refresh_theme(app);
        app->settings_dirty = 1;
        save_settings(app);
    }
    *y += ui_theme_picker_height(w) + flint_px(20);
}

void
settings_theme_handle_overlays(InbeApp *app, const SettingsThemeState *state)
{
    int theme_mode_changed = 0;

    if(app == NULL || state == NULL)
        return;
    if(state->draw_theme_mode_menu && ui_draw_dropdown_menu(102))
        theme_mode_changed = 1;
    if(theme_mode_changed) {
        app->theme_mode = clampi(app->theme_mode, APP_THEME_SYSTEM, APP_THEME_DARK);
        app_refresh_theme(app);
        app->settings_dirty = 1;
        save_settings(app);
    }
}
