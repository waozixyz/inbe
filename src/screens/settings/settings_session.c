#include "settings_session.h"

#include "app.h"
#include "app_settings.h"
#include "flint_locale.h"
#include "flint_ui.h"

#define SETTINGS_SESSION_SLIDER_ROW_H 52

static FlintUIParagraph
settings_session_label_paragraph(const char *label, int w)
{
    return (FlintUIParagraph){
        .text = label,
        .width = w
    };
}

static int
settings_session_toggle_row_height(const char *label, int w)
{
    int label_h;
    int row_h;

    if(w <= 0)
        w = flint_px(160);

    label_h = flint_ui_paragraph_height(settings_session_label_paragraph(label, w));
    row_h = label_h + flint_px(8) + flint_px(30) + flint_px(22);
    if(row_h < flint_px(76))
        row_h = flint_px(76);
    return row_h;
}

static int
settings_session_draw_toggle_row(int x, int w, int *y, const char *label, int *value)
{
    int label_y;
    int label_h;
    int row_h;
    int toggle_w = flint_px(56);
    int toggle_h = flint_px(30);

    if(y == NULL)
        return 0;

    row_h = settings_session_toggle_row_height(label, w);
    label_y = *y;
    flint_ui_paragraph_draw(settings_session_label_paragraph(label, w), x, &label_y);
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
settings_session_content_height(int content_w)
{
    int label_w = content_w > 0 ? content_w : flint_px(240);
    int height = flint_px(SETTINGS_SESSION_SLIDER_ROW_H) + flint_px(40);

    height += settings_session_toggle_row_height(locale_get("show_session_volume_control_label"),
                                                 label_w);
    height += settings_session_toggle_row_height(locale_get("play_in_background_label"),
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
    if(settings_session_draw_toggle_row(x, w, y,
                                        locale_get("show_session_volume_control_label"),
                                        &show_session_volume)) {
        app->show_session_volume_control = show_session_volume;
        app->settings_dirty = 1;
        save_settings(app);
    }

    play_in_background = app->inbe.play_in_background;
    if(settings_session_draw_toggle_row(x, w, y, locale_get("play_in_background_label"),
                                        &play_in_background)) {
        app->inbe.play_in_background = play_in_background;
        app->settings_dirty = 1;
        save_settings(app);
    }
}
