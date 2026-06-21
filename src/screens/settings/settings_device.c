#include "settings_device.h"

#include "app.h"
#include "app_settings.h"
#include "device_preferences.h"
#include "language_screen.h"
#include "flint_locale.h"
#include "flint_theme.h"
#include "flint_ui.h"

static int
settings_device_orientation_option_count(void)
{
#if INBE_ANDROID_BUILD
    return 4;
#else
    return 3;
#endif
}

static FlintUIParagraph
settings_device_label_paragraph(const char *label, int w)
{
    return (FlintUIParagraph){
        .text = label,
        .width = w
    };
}

static int
settings_device_toggle_row_height(const char *label, int w)
{
    int label_h;
    int row_h;

    if(w <= 0)
        w = flint_px(160);

    label_h = flint_ui_paragraph_height(settings_device_label_paragraph(label, w));
    row_h = label_h + flint_px(8) + flint_px(30) + flint_px(22);
    if(row_h < flint_px(76))
        row_h = flint_px(76);
    return row_h;
}

static int
settings_device_draw_toggle_row(int x, int w, int *y, const char *label, int *value)
{
    int label_y;
    int label_h;
    int row_h;
    int toggle_w = flint_px(56);
    int toggle_h = flint_px(30);

    if(y == NULL)
        return 0;

    row_h = settings_device_toggle_row_height(label, w);
    label_y = *y;
    flint_ui_paragraph_draw(settings_device_label_paragraph(label, w), x, &label_y);
    label_h = label_y - *y;

    if(ui_draw_toggle_switch(x, *y + label_h + flint_px(8), toggle_w, toggle_h,
                             value, locale_get("toggle_off"),
                             locale_get("toggle_on"))) {
        *y += row_h;
        return 1;
    }

    *y += row_h;
    return 0;
}

int
settings_device_content_height(int content_w)
{
    int height = flint_px(74) + flint_px(74) + flint_px(76) + flint_px(40);
    int label_w = content_w > 0 ? content_w : flint_px(240);

#if INBE_ANDROID_BUILD || defined(PLATFORM_WEB)
    height += settings_device_toggle_row_height(locale_get("play_in_background_label"), label_w);
#endif
#if !INBE_ANDROID_BUILD && !defined(PLATFORM_WEB)
    height += flint_px(50);
#endif
    height += settings_device_toggle_row_height(locale_get("show_session_volume_control_label"), label_w);
    height += settings_device_toggle_row_height(locale_get("on_screen_keyboard_label"), label_w);
    return height;
}

void
settings_device_draw(InbeApp *app, int x, int w, int *y, SettingsDeviceState *state)
{
    int sound_volume;
    int show_session_volume;
    int keyboard_toggle;
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
    show_session_volume = app->show_session_volume_control;
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

    if(settings_device_draw_toggle_row(x, w, y, locale_get("show_session_volume_control_label"),
                                       &show_session_volume)) {
        app->show_session_volume_control = show_session_volume;
        app->settings_dirty = 1;
        save_settings(app);
    }

#if INBE_ANDROID_BUILD || defined(PLATFORM_WEB)
    {
        int play_in_background = app->inbe.play_in_background;
        if(settings_device_draw_toggle_row(x, w, y, locale_get("play_in_background_label"),
                                           &play_in_background)) {
            app->inbe.play_in_background = play_in_background;
            app->settings_dirty = 1;
        }
    }
#endif

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

#if !INBE_ANDROID_BUILD && !defined(PLATFORM_WEB)
    if(ui_draw_checkbox_toggle(x, *y, locale_get("fullscreen_label"), &app->fullscreen_enabled)) {
        if(app->fullscreen_enabled && !IsWindowFullscreen())
            ToggleFullscreen();
        else if(!app->fullscreen_enabled && IsWindowFullscreen())
            ToggleFullscreen();
        app->settings_dirty = 1;
    }
    *y += flint_px(50);
#endif

    if(settings_device_draw_toggle_row(x, w, y, locale_get("on_screen_keyboard_label"),
                                       &keyboard_toggle)) {
        app->on_screen_keyboard_enabled = keyboard_toggle;
        app->settings_dirty = 1;
    }
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
