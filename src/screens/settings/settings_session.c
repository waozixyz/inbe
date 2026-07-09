#include "settings_session.h"

#include "app.h"
#include "app_settings.h"
#include "locale.h"
#include "ui.h"
#include "settings_ui.h"

#define SETTINGS_SESSION_SLIDER_ROW_H 52

int
settings_session_content_height(int content_w)
{
    int label_w = content_w > 0 ? content_w : ScaleUIPx(240);
    int height = ScaleUIPx(SETTINGS_SESSION_SLIDER_ROW_H) + ScaleUIPx(40);

    height += settings_ui_toggle_row_height(GetLocaleText("show_session_volume_control_label"),
                                            label_w);
    height += settings_ui_toggle_row_height(GetLocaleText("show_session_return_button_label"),
                                            label_w);
    height += settings_ui_toggle_row_height(GetLocaleText("play_in_background_label"),
                                            label_w);
    return height;
}

void
settings_session_draw(InbeApp *app, int x, int w, int *y)
{
    int sound_volume;
    int show_session_volume;
    int show_session_return;
    int play_in_background;

    if(app == NULL || y == NULL)
        return;

    sound_volume = app->sound_volume;
    if(settings_ui_commit_slider_row(app, 6, x, w, y,
                                     ScaleUIPx(SETTINGS_SESSION_SLIDER_ROW_H),
                                     GetLocaleText("volume_label"),
                                     SETTINGS_VOLUME_MIN, SETTINGS_VOLUME_MAX,
                                     &sound_volume, "", 0)) {
        app->sound_volume = sound_volume;
        save_settings(app);
    }

    show_session_volume = app->show_session_volume_control;
    if(settings_ui_commit_toggle_row(app, x, w, y,
                                     GetLocaleText("show_session_volume_control_label"),
                                     &show_session_volume, 0)) {
        app->show_session_volume_control = show_session_volume;
        save_settings(app);
    }

    show_session_return = app->show_session_return_button;
    if(settings_ui_commit_toggle_row(app, x, w, y,
                                     GetLocaleText("show_session_return_button_label"),
                                     &show_session_return, 0)) {
        app->show_session_return_button = show_session_return;
        save_settings(app);
    }

    play_in_background = app->inbe.play_in_background;
    if(settings_ui_commit_toggle_row(app, x, w, y,
                                     GetLocaleText("play_in_background_label"),
                                     &play_in_background, 0)) {
        app->inbe.play_in_background = play_in_background;
        save_settings(app);
    }
}
