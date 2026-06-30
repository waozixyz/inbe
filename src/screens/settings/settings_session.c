#include "settings_session.h"

#include "app.h"
#include "app_settings.h"
#include "flint_locale.h"
#include "flint_ui.h"
#include "settings_ui.h"

#define SETTINGS_SESSION_SLIDER_ROW_H 52

int
settings_session_content_height(int content_w)
{
    int label_w = content_w > 0 ? content_w : flint_px(240);
    int height = flint_px(SETTINGS_SESSION_SLIDER_ROW_H) + flint_px(40);

    height += settings_ui_toggle_row_height(locale_get("show_session_volume_control_label"),
                                            label_w);
    height += settings_ui_toggle_row_height(locale_get("play_in_background_label"),
                                            label_w);
    return height;
}

void
settings_session_draw(InbeApp *app, int x, int w, int *y)
{
    int sound_volume;
    int show_session_volume;
    int play_in_background;

    if(app == NULL || y == NULL)
        return;

    sound_volume = app->sound_volume;
    if(ui_draw_slider(6, x, *y, w, locale_get("volume_label"),
                      SETTINGS_VOLUME_MIN, SETTINGS_VOLUME_MAX, &sound_volume, "")) {
        app->sound_volume = sound_volume;
        app->settings_dirty = 1;
        save_settings(app);
    }
    *y += flint_px(SETTINGS_SESSION_SLIDER_ROW_H);

    show_session_volume = app->show_session_volume_control;
    if(settings_ui_draw_toggle_row(x, w, y,
                                   locale_get("show_session_volume_control_label"),
                                   &show_session_volume)) {
        app->show_session_volume_control = show_session_volume;
        app->settings_dirty = 1;
        save_settings(app);
    }

    play_in_background = app->inbe.play_in_background;
    if(settings_ui_draw_toggle_row(x, w, y, locale_get("play_in_background_label"),
                                   &play_in_background)) {
        app->inbe.play_in_background = play_in_background;
        app->settings_dirty = 1;
        save_settings(app);
    }
}
