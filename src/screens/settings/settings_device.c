#include "settings_device.h"

#include "app.h"
#include "app_settings.h"
#include "device_preferences.h"
#include "language_screen.h"
#include "flint_locale.h"
#include "flint_theme.h"
#include "flint_ui.h"
#include "settings_ui.h"

static int
settings_device_orientation_option_count(void)
{
#if ANDROID_BUILD
    return 4;
#else
    return 3;
#endif
}

int
settings_device_content_height(int content_w)
{
    int height = flint_px(76) + flint_px(76) + flint_px(40);
#if !ANDROID_BUILD && !defined(PLATFORM_WEB)
    int label_w = content_w > 0 ? content_w : flint_px(240);
#endif

#if !ANDROID_BUILD && !defined(PLATFORM_WEB)
    height += flint_px(50);
#endif
#if !ANDROID_BUILD && !defined(PLATFORM_WEB)
    height += settings_ui_toggle_row_height(locale_get("on_screen_keyboard_label"), label_w);
#endif
    return height;
}

void
settings_device_draw(InbeApp *app, int x, int w, int *y, SettingsDeviceState *state)
{
#if !ANDROID_BUILD && !defined(PLATFORM_WEB)
    int keyboard_toggle;
#endif
    const char *orientation_options[] = {
        locale_get("orientation_system"),
        locale_get("orientation_portrait"),
        locale_get("orientation_landscape"),
        locale_get("orientation_sensor")
    };
    int orientation_option_count = settings_device_orientation_option_count();

    if(app == NULL || y == NULL || state == NULL)
        return;

#if ANDROID_BUILD || defined(PLATFORM_WEB)
    app->on_screen_keyboard_enabled = 1;
#else
    keyboard_toggle = app->on_screen_keyboard_enabled;
#endif

    flint_text_draw(locale_get("language_label"), x, *y, flint_ui_font(), flint_theme_get_text());
    if(language_dropdown_button(app, 101, x, *y + flint_px(26), w, flint_px(36),
                                &app->language_index))
        state->language_menu_changed = 1;
    state->draw_language_menu = 1;
    *y += flint_px(76);

    {
        int orientation_max = orientation_option_count - 1;
        app->orientation_mode = clampi(app->orientation_mode,
                                       APP_ORIENTATION_SYSTEM,
                                       orientation_max);
        flint_text_draw(locale_get("orientation_label"), x, *y,
                        flint_ui_font(), flint_theme_get_text());
        ui_draw_dropdown_button(103, x, *y + flint_px(26), w, flint_px(36),
                                orientation_options, orientation_option_count,
                                &app->orientation_mode);
        state->draw_orientation_menu = 1;
        *y += flint_px(76);
    }

#if !ANDROID_BUILD && !defined(PLATFORM_WEB)
    if(ui_draw_checkbox_toggle(x, *y, locale_get("fullscreen_label"), &app->fullscreen_enabled)) {
        if(app->fullscreen_enabled && !IsWindowFullscreen())
            ToggleFullscreen();
        else if(!app->fullscreen_enabled && IsWindowFullscreen())
            ToggleFullscreen();
        app->settings_dirty = 1;
    }
    *y += flint_px(50);
#endif

#if !ANDROID_BUILD && !defined(PLATFORM_WEB)
    if(settings_ui_commit_toggle_row(app, x, w, y, locale_get("on_screen_keyboard_label"),
                                     &keyboard_toggle, 0)) {
        app->on_screen_keyboard_enabled = keyboard_toggle;
    }
#endif
}

void
settings_device_handle_overlays(InbeApp *app, SettingsDeviceState *state)
{
    int orientation_option_count = settings_device_orientation_option_count();

    if(app == NULL || state == NULL)
        return;
    if(state->draw_language_menu && language_dropdown_menu(app, 101))
        state->language_menu_changed = 1;
    if(state->draw_orientation_menu && ui_draw_dropdown_menu(103))
        state->orientation_changed = 1;
    if(state->language_menu_changed)
        apply_language_selection(app, app->language_index, 1);
    if(state->orientation_changed) {
        int orientation_max = orientation_option_count - 1;
        app->orientation_mode = clampi(app->orientation_mode, APP_ORIENTATION_SYSTEM,
                                       orientation_max);
        app_apply_orientation_preference(app);
        app->settings_dirty = 1;
        save_settings(app);
    }
}
