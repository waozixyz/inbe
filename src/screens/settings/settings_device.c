#include "settings_device.h"

#include "app.h"
#include "app_settings.h"
#include "device_preferences.h"
#include "language_screen.h"
#include "flint_locale.h"
#include "theme.h"
#include "flint_ui.h"

static int
settings_device_orientation_option_count(void)
{
#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
    return 4;
#else
    return 3;
#endif
}

int
settings_device_content_height(void)
{
    int height = flint_px(74) + flint_px(74) + flint_px(76) + flint_px(40);

#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
    height += flint_px(76);
#endif
#if !defined(PLATFORM_ANDROID) && !defined(__ANDROID__) && !defined(ANDROID) && !defined(PLATFORM_WEB)
    height += flint_px(50);
#endif
    height += flint_px(72);
    return height;
}

void
settings_device_draw(InbeApp *app, int x, int w, int *y, SettingsDeviceState *state)
{
    int sound_volume;
    int keyboard_toggle;
    int toggle_w = flint_px(56);
    int toggle_h = flint_px(30);
    const char *orientation_options[] = {
        locale_get("orientation_system"),
        locale_get("orientation_portrait"),
        locale_get("orientation_landscape"),
        locale_get("orientation_sensor")
    };
    int orientation_option_count = settings_device_orientation_option_count();

    if(app == NULL || y == NULL || state == NULL)
        return;

    sound_volume = app->sound_volume;
    keyboard_toggle = app->on_screen_keyboard_enabled;

    if(language_dropdown_button(app, 101, x, *y, w, flint_px(36), &app->language_index))
        state->language_menu_changed = 1;
    state->draw_language_menu = 1;
    *y += flint_px(74);

    if(ui_draw_slider(6, x, *y, w, locale_get("volume_label"),
                      SETTINGS_VOLUME_MIN, SETTINGS_VOLUME_MAX, &sound_volume, "")) {
        app->sound_volume = sound_volume;
        app->settings_dirty = 1;
        save_settings(app);
    }
    *y += flint_px(74);

#ifdef __ANDROID__
    {
        int play_in_background = app->inbe.play_in_background;
        flint_text_draw(locale_get("play_in_background_label"), x, *y,
                        flint_ui_font(), theme_get_text());
        if(ui_draw_toggle_switch(x, *y + flint_px(26), toggle_w, toggle_h,
                                 &play_in_background, locale_get("toggle_off"),
                                 locale_get("toggle_on"))) {
            app->inbe.play_in_background = play_in_background;
            app->settings_dirty = 1;
        }
        *y += flint_px(76);
    }
#endif

    {
        int orientation_max = orientation_option_count - 1;
        app->orientation_mode = clampi(app->orientation_mode,
                                       APP_ORIENTATION_SYSTEM,
                                       orientation_max);
        flint_text_draw(locale_get("orientation_label"), x, *y,
                        flint_ui_font(), theme_get_text());
        ui_draw_dropdown_button(103, x, *y + flint_px(26), w, flint_px(36),
                                orientation_options, orientation_option_count,
                                &app->orientation_mode);
        state->draw_orientation_menu = 1;
        *y += flint_px(76);
    }

#if !defined(PLATFORM_ANDROID) && !defined(__ANDROID__) && !defined(ANDROID) && !defined(PLATFORM_WEB)
    if(ui_draw_checkbox_toggle(x, *y, locale_get("fullscreen_label"), &app->fullscreen_enabled)) {
        if(app->fullscreen_enabled && !IsWindowFullscreen())
            ToggleFullscreen();
        else if(!app->fullscreen_enabled && IsWindowFullscreen())
            ToggleFullscreen();
        app->settings_dirty = 1;
    }
    *y += flint_px(50);
#endif

    flint_text_draw(locale_get("on_screen_keyboard_label"), x, *y,
                    flint_ui_font(), theme_get_text());
    if(ui_draw_toggle_switch(x, *y + flint_px(26), toggle_w, toggle_h,
                             &keyboard_toggle, locale_get("toggle_off"),
                             locale_get("toggle_on"))) {
        app->on_screen_keyboard_enabled = keyboard_toggle;
        app->settings_dirty = 1;
    }
    *y += flint_px(76);
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
