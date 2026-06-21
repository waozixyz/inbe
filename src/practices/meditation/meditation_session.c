#include "meditation_practice.h"

#include "app.h"
#include "data.h"
#include "flint_locale.h"
#include "meditation_music.h"
#include "theme.h"
#include "flint_ui.h"
#include "raylib.h"

#include <stdio.h>

extern int view_width;
extern int view_height;

static Texture2D
meditation_sound_icon_for_volume(InbeApp *app)
{
    int vol = app->sound_volume;
    if(vol <= 0)
        return app->icons[UI_ICON_TYPE_SOUND0];
    if(vol <= 25)
        return app->icons[UI_ICON_TYPE_SOUND1];
    if(vol <= 75)
        return app->icons[UI_ICON_TYPE_SOUND2];
    return app->icons[UI_ICON_TYPE_SOUND3];
}

static void
meditation_start(InbeApp *app, int seconds)
{
    if(app == NULL)
        return;

    app->meditation.duration_seconds = seconds;
    app->meditation.remaining_seconds = seconds;
    app->meditation.frame_ticks = 0;
    app->session_paused = 0;
    app->volume_popup_active = 0;
    app->modal.active = 0;
    app->modal.type = UIModalNone;
    app_switch_screen(app, InbeScreenMeditation);
    app_play_sound(app, app->bell_sound, 1.0f);
    meditation_music_start_session(app);
}

static void
meditation_finish(InbeApp *app)
{
    int duration;

    if(app == NULL)
        return;

    duration = app->meditation.duration_seconds;
    if(duration >= 60) {
        int session_duration = duration;
        if(data_save_session_path_for_activity(&session_duration, 1, 0, EXERCISE_MEDITATION,
                                               NULL, 0)) {
            sync_habits_for_activity(app, EXERCISE_MEDITATION);
            app_auto_sync(app);
        }
    }
    app_play_sound(app, app->bell_sound, 1.0f);
    app->meditation.duration_seconds = 0;
    app->meditation.remaining_seconds = 0;
    app->meditation.frame_ticks = 0;
    app->session_paused = 0;
    app->volume_popup_active = 0;
    meditation_music_stop(app);
    app_switch_screen(app, InbeScreenStart);
}

void
meditation_background_tick(InbeApp *app, int elapsed_ms)
{
    int elapsed_seconds;

    if(app == NULL || elapsed_ms <= 0)
        return;
    if(app->inbe.screen != InbeScreenMeditation || app->session_paused)
        return;

    elapsed_seconds = elapsed_ms / 1000;
    if(elapsed_seconds <= 0)
        return;

    if(elapsed_seconds >= app->meditation.remaining_seconds)
        app->meditation.remaining_seconds = 0;
    else
        app->meditation.remaining_seconds -= elapsed_seconds;
    app->meditation.frame_ticks = 0;

    if(app->meditation.remaining_seconds <= 0)
        meditation_finish(app);
}

static int
meditation_elapsed_seconds(InbeApp *app)
{
    int elapsed;

    if(app == NULL)
        return 0;
    elapsed = app->meditation.duration_seconds - app->meditation.remaining_seconds;
    if(elapsed < 0)
        elapsed = 0;
    if(elapsed > app->meditation.duration_seconds)
        elapsed = app->meditation.duration_seconds;
    return elapsed;
}

static int
meditation_save_elapsed(InbeApp *app)
{
    int elapsed;

    if(app == NULL)
        return 0;
    elapsed = meditation_elapsed_seconds(app);
    if(elapsed < 60)
        return 0;
    if(data_save_session_path_for_activity(&elapsed, 1, 0, EXERCISE_MEDITATION, NULL, 0)) {
        sync_habits_for_activity(app, EXERCISE_MEDITATION);
        app_auto_sync(app);
        return 1;
    }
    return 0;
}

static void
meditation_exit_to_start(InbeApp *app)
{
    if(app == NULL)
        return;
    meditation_music_stop(app);
    app->meditation.duration_seconds = 0;
    app->meditation.remaining_seconds = 0;
    app->meditation.frame_ticks = 0;
    app->session_paused = 0;
    app->volume_popup_active = 0;
    app->modal.active = 0;
    app->modal.type = UIModalNone;
    app_switch_screen(app, InbeScreenStart);
}

void
meditation_request_exit(InbeApp *app)
{
    if(app == NULL)
        return;
    app->modal.active = 1;
    app->modal.type = UIModalConfirmExitSession;
    app->modal.selected_button = 0;
}

static void
format_meditation_time(char *dst, size_t dst_size, int seconds)
{
    int hours;
    int minutes;
    int secs;

    if(dst == NULL || dst_size == 0)
        return;
    if(seconds < 0)
        seconds = 0;

    hours = seconds / 3600;
    minutes = (seconds / 60) % 60;
    secs = seconds % 60;

    if(hours > 0)
        snprintf(dst, dst_size, "%d:%02d:%02d", hours, minutes, secs);
    else
        snprintf(dst, dst_size, "%d:%02d", minutes, secs);
}

static int
draw_meditation_duration_button(int x, int y, int w, int h, const char *label)
{
    int hover = 0;
    return ui_draw_generic_button(x, y, w, h, label, UI_BUTTON_STYLE_PRIMARY, 0, &hover);
}

void
meditation_draw_setup_modal(InbeApp *app)
{
    static const int durations[] = {5 * 60, 15 * 60, 30 * 60, 60 * 60, 2 * 60 * 60};
    const char *labels[] = {
        locale_get("duration_5m"),
        locale_get("duration_15m"),
        locale_get("duration_30m"),
        locale_get("duration_1h"),
        locale_get("duration_2h")
    };
    int modal_w = flint_px(320);
    int modal_h = flint_px(236);
    int modal_x;
    int modal_y;
    int title_font;
    int title_w;
    int btn_h = flint_px(38);
    int gap = flint_px(10);
    int side = flint_px(18);
    int row_y;
    int btn_w;
    int cancel_w = flint_px(120);
    int cancel_x;
    int cancel_y;
    int cancel_hover = 0;

    if(modal_w > view_width - flint_px(24))
        modal_w = view_width - flint_px(24);
    if(modal_h > view_height - flint_px(24))
        modal_h = view_height - flint_px(24);

    modal_x = (view_width - modal_w) / 2;
    modal_y = (view_height - modal_h) / 2;
    btn_w = (modal_w - side * 2 - gap * 2) / 3;
    if(btn_w < flint_px(64))
        btn_w = flint_px(64);

    DrawRectangle(0, 0, view_width, view_height, (Color){0, 0, 0, 180});
    DrawRectangle(modal_x, modal_y, modal_w, modal_h, theme_get_surface());
    ui_draw_bevel(modal_x, modal_y, modal_w, modal_h,
                  flint_lighten(theme_get_surface(), 40), flint_darken(theme_get_surface(), 40));

    title_font = flint_ui_title_font(locale_get("meditation_title"), modal_w - side * 2);
    title_w = flint_text_measure(locale_get("meditation_title"), title_font);
    flint_text_draw(locale_get("meditation_title"), modal_x + (modal_w - title_w) / 2,
                    modal_y + flint_px(16), title_font, theme_get_text());

    row_y = modal_y + flint_px(62);
    for(int i = 0; i < 3; i++) {
        int x = modal_x + side + i * (btn_w + gap);
        if(draw_meditation_duration_button(x, row_y, btn_w, btn_h, labels[i]))
            meditation_start(app, durations[i]);
    }

    row_y += btn_h + gap;
    for(int i = 3; i < 5; i++) {
        int two_w = (modal_w - side * 2 - gap) / 2;
        int x = modal_x + side + (i - 3) * (two_w + gap);
        if(draw_meditation_duration_button(x, row_y, two_w, btn_h, labels[i]))
            meditation_start(app, durations[i]);
    }

    cancel_y = modal_y + modal_h - btn_h - flint_px(16);
    cancel_x = modal_x + (modal_w - cancel_w) / 2;
    if(ui_draw_generic_button(cancel_x, cancel_y, cancel_w, btn_h,
                              locale_get("cancel_button"), UI_BUTTON_STYLE_SECONDARY,
                              0, &cancel_hover)) {
        app->modal.active = 0;
        app->modal.type = UIModalNone;
    }
}

static void
draw_meditation_sound_controls(InbeApp *app)
{
    if(app->show_session_volume_control) {
        if(ui_draw_icon_slider_popup((FlintUIIconSliderPopup){
               .id = 501,
               .x = view_width - flint_px(56),
               .y = flint_px(12),
               .icon_size = flint_px(24),
               .icon_padding = flint_px(10),
               .icon = meditation_sound_icon_for_volume(app),
               .open = &app->volume_popup_active,
               .value = &app->sound_volume,
               .min = SETTINGS_VOLUME_MIN,
               .max = SETTINGS_VOLUME_MAX,
               .popup_width = flint_px(44),
               .popup_height = flint_px(200)
           })) {
            app->settings_dirty = 1;
            save_settings(app);
        }
    } else {
        app->volume_popup_active = 0;
    }
}

void
meditation_draw_screen(InbeApp *app, int center_x, int center_y)
{
    char time_text[32];
    int return_hover = 0;
    int font = FLINT_TEXT_24;
    int max_w;
    int text_w;

    if(ui_draw_icon_btn_padded(flint_px(12), flint_px(12), flint_px(24),
                               flint_px(10), app->icons[UI_ICON_TYPE_RETURN], &return_hover)) {
        meditation_request_exit(app);
        return;
    }

    draw_meditation_sound_controls(app);

    if(app->modal.active && app->modal.type == UIModalConfirmExitSession) {
        int elapsed = meditation_elapsed_seconds(app);
        int modal_result;

        if(elapsed >= 60) {
            modal_result = ui_draw_modal_3btn(locale_get("exit_session_title"),
                                              locale_get("meditation_save_elapsed_message"),
                                              locale_get("cancel_button"),
                                              locale_get("save_button"),
                                              locale_get("discard_button"));
            if(modal_result == 1) {
                app->modal.active = 0;
                app->modal.type = UIModalNone;
            } else if(modal_result == 2) {
                meditation_save_elapsed(app);
                meditation_exit_to_start(app);
            } else if(modal_result == 3) {
                meditation_exit_to_start(app);
            }
        } else {
            modal_result = ui_draw_modal(locale_get("exit_session_title"),
                                         locale_get("meditation_under_minute_exit_message"),
                                         locale_get("cancel_button"),
                                         locale_get("exit_button"));
            if(modal_result == 1) {
                app->modal.active = 0;
                app->modal.type = UIModalNone;
            } else if(modal_result == 2) {
                meditation_exit_to_start(app);
            }
        }
        return;
    }

    format_meditation_time(time_text, sizeof(time_text), app->meditation.remaining_seconds);
    max_w = view_width - flint_px(48);
    if(flint_text_measure(time_text, font) > max_w)
        font = FLINT_TEXT_16;
    text_w = flint_text_measure(time_text, font);
    flint_text_draw(time_text, center_x - text_w / 2,
                    flint_ui_text_y(time_text, center_y - font, font * 2, font),
                    font, theme_get_text());

    if(!(
#if defined(PLATFORM_WEB)
        app->backgrounded
#else
        0
#endif
    )) {
        app->meditation.frame_ticks++;
        if(app->meditation.frame_ticks >= 60) {
            app->meditation.frame_ticks = 0;
            if(app->meditation.remaining_seconds > 0)
                app->meditation.remaining_seconds--;
            if(app->meditation.remaining_seconds <= 0)
                meditation_finish(app);
        }
    }
}
