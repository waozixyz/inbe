#include "settings_device.h"

#include "app.h"
#include "app_settings.h"
#include "device_preferences.h"
#include "language_screen.h"
#include "locale.h"
#include "theme.h"
#include "ui.h"
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
    int height = ScaleUIPx(76) + ScaleUIPx(76) + ScaleUIPx(40);
#if !ANDROID_BUILD && !defined(PLATFORM_WEB)
    int label_w = content_w > 0 ? content_w : ScaleUIPx(240);
#endif

#if !ANDROID_BUILD && !defined(PLATFORM_WEB)
    height += ScaleUIPx(50);
#endif
#if !ANDROID_BUILD && !defined(PLATFORM_WEB)
    height += settings_ui_toggle_row_height(GetLocaleText("on_screen_keyboard_label"), label_w);
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
        GetLocaleText("orientation_system"),
        GetLocaleText("orientation_portrait"),
        GetLocaleText("orientation_landscape"),
        GetLocaleText("orientation_sensor")
    };
    int orientation_option_count = settings_device_orientation_option_count();

    if(app == NULL || y == NULL || state == NULL)
        return;

#if ANDROID_BUILD || defined(PLATFORM_WEB)
    app->on_screen_keyboard_enabled = 1;
#else
    keyboard_toggle = app->on_screen_keyboard_enabled;
#endif

    DrawUIText(GetLocaleText("language_label"), x, *y, GetUIFontSize(), GetThemeText());
    if(language_dropdown_button(app, 101, x, *y + ScaleUIPx(26), w, ScaleUIPx(36),
                                &app->language_index))
        state->language_menu_changed = 1;
    state->draw_language_menu = 1;
    *y += ScaleUIPx(76);

    {
        int orientation_max = orientation_option_count - 1;
        app->orientation_mode = clampi(app->orientation_mode,
                                       APP_ORIENTATION_SYSTEM,
                                       orientation_max);
        DrawUIText(GetLocaleText("orientation_label"), x, *y,
                        GetUIFontSize(), GetThemeText());
        DrawUIDropdownButton(103, x, *y + ScaleUIPx(26), w, ScaleUIPx(36),
                                orientation_options, orientation_option_count,
                                &app->orientation_mode);
        state->draw_orientation_menu = 1;
        *y += ScaleUIPx(76);
    }

#if !ANDROID_BUILD && !defined(PLATFORM_WEB)
    if(DrawUICheckboxToggle(x, *y, GetLocaleText("fullscreen_label"), &app->fullscreen_enabled)) {
        if(app->fullscreen_enabled && !IsWindowFullscreen())
            ToggleFullscreen();
        else if(!app->fullscreen_enabled && IsWindowFullscreen())
            ToggleFullscreen();
        app->settings_dirty = 1;
    }
    *y += ScaleUIPx(50);
#endif

#if !ANDROID_BUILD && !defined(PLATFORM_WEB)
    if(settings_ui_commit_toggle_row(app, x, w, y, GetLocaleText("on_screen_keyboard_label"),
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
    if(state->draw_orientation_menu && DrawUIDropdownMenu(103))
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
