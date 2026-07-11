#include "meditation_practice.h"

#include "app.h"
#include "data.h"
#include "locale.h"
#include "meditation_music.h"
#include "theme.h"
#include "ui.h"
#include "practices/practice_registry.h"
#include "flint.h"

#include <stdio.h>

extern int view_width;
extern int view_height;

static int meditation_background_remainder_ms = 0;

static const int meditation_duration_presets[] = {
    5 * 60,
    15 * 60,
    30 * 60,
    60 * 60,
    2 * 60 * 60
};

static void meditation_timer_complete(InbeApp *app);

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

    if(seconds < 60)
        seconds = 60;

    app->meditation.duration_seconds = seconds;
    app->meditation.remaining_seconds = seconds;
    app->meditation.frame_ticks = 0;
    app->meditation.complete_waiting = 0;
    meditation_background_remainder_ms = 0;
    app->session_paused = 0;
    app->volume_popup_active = 0;
    app_close_modal(app);
    app_switch_screen(app, InbeScreenMeditation);
    app_play_bell_cue(app, 1.0f);
    meditation_music_start_session(app);
    practice_background_start(app, PRACTICE_MEDITATION);
}

int
meditation_configured_duration_seconds(const InbeApp *app)
{
    int mode;

    if(app == NULL)
        return meditation_duration_presets[1];

    mode = app->meditation.duration_mode;
    if(mode >= 0 && mode < (int)(sizeof(meditation_duration_presets) /
                                 sizeof(meditation_duration_presets[0])))
        return meditation_duration_presets[mode];

    return clampi(app->meditation.custom_minutes, 1, 240) * 60;
}

void
meditation_start_configured(InbeApp *app)
{
    meditation_start(app, meditation_configured_duration_seconds(app));
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
    if(!app->meditation.complete_waiting)
        app_play_bell_cue(app, 1.0f);
    app->meditation.duration_seconds = 0;
    app->meditation.remaining_seconds = 0;
    app->meditation.frame_ticks = 0;
    app->meditation.complete_waiting = 0;
    meditation_background_remainder_ms = 0;
    app->session_paused = 0;
    app->volume_popup_active = 0;
    meditation_music_stop(app);
    practice_active_background_stop(app);
    app_switch_screen(app, InbeScreenStart);
}

static void
meditation_timer_complete(InbeApp *app)
{
    if(app == NULL)
        return;

    if(app->meditation.show_extend_controls) {
        app->meditation.remaining_seconds = 0;
        app->meditation.frame_ticks = 0;
        if(!app->meditation.complete_waiting) {
            app->meditation.complete_waiting = 1;
            app_play_bell_cue(app, 1.0f);
        }
        return;
    }

    meditation_finish(app);
}

void
meditation_advance_elapsed(InbeApp *app, int elapsed_ms)
{
    int elapsed_total;
    int elapsed_seconds;

    if(app == NULL || elapsed_ms <= 0)
        return;
    if(app->inbe.screen != InbeScreenMeditation || app->session_paused) {
        meditation_background_remainder_ms = 0;
        return;
    }

    elapsed_total = elapsed_ms + meditation_background_remainder_ms;
    elapsed_seconds = elapsed_total / 1000;
    meditation_background_remainder_ms = elapsed_total % 1000;
    if(elapsed_seconds <= 0)
        return;

    if(elapsed_seconds >= app->meditation.remaining_seconds)
        app->meditation.remaining_seconds = 0;
    else
        app->meditation.remaining_seconds -= elapsed_seconds;
    app->meditation.frame_ticks = 0;

    if(app->meditation.remaining_seconds <= 0)
        meditation_timer_complete(app);
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
    app->meditation.complete_waiting = 0;
    meditation_background_remainder_ms = 0;
    app->session_paused = 0;
    app->volume_popup_active = 0;
    app_close_modal(app);
    practice_active_background_stop(app);
    app_switch_screen(app, InbeScreenStart);
}

void
meditation_request_exit(InbeApp *app)
{
    if(app == NULL)
        return;
    app_open_modal(app, UIModalConfirmExitSession);
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
    return DrawUIGenericButton(x, y, w, h, label, UI_BUTTON_STYLE_PRIMARY, 0, &hover);
}

static void
meditation_extend_session(InbeApp *app, int seconds)
{
    if(app == NULL || seconds <= 0)
        return;

    app->meditation.duration_seconds += seconds;
    app->meditation.remaining_seconds += seconds;
    app->meditation.complete_waiting = 0;
    app->meditation.frame_ticks = 0;
}

void
meditation_draw_setup_modal(InbeApp *app)
{
    static const int durations[] = {5 * 60, 15 * 60, 30 * 60, 60 * 60, 2 * 60 * 60};
    const char *labels[] = {
        GetLocaleText("duration_5m"),
        GetLocaleText("duration_15m"),
        GetLocaleText("duration_30m"),
        GetLocaleText("duration_1h"),
        GetLocaleText("duration_2h")
    };
    int modal_w = ScaleUIPx(320);
    int modal_h = ScaleUIPx(236);
    int modal_x;
    int modal_y;
    int title_font;
    int title_w;
    int btn_h = ScaleUIPx(38);
    int gap = ScaleUIPx(10);
    int side = ScaleUIPx(18);
    int row_y;
    int btn_w;
    int cancel_w = ScaleUIPx(120);
    int cancel_x;
    int cancel_y;
    int cancel_hover = 0;

    if(modal_w > view_width - ScaleUIPx(24))
        modal_w = view_width - ScaleUIPx(24);
    if(modal_h > view_height - ScaleUIPx(24))
        modal_h = view_height - ScaleUIPx(24);

    modal_x = (view_width - modal_w) / 2;
    modal_y = (view_height - modal_h) / 2;
    btn_w = (modal_w - side * 2 - gap * 2) / 3;
    if(btn_w < ScaleUIPx(64))
        btn_w = ScaleUIPx(64);

    SetUIModalCapture((Rectangle){
        (float)modal_x, (float)modal_y, (float)modal_w, (float)modal_h
    });
    DrawRectangle(0, 0, view_width, view_height, (Color){0, 0, 0, 180});
    DrawRectangle(modal_x, modal_y, modal_w, modal_h, GetThemeSurface());
    DrawUIBevel(modal_x, modal_y, modal_w, modal_h,
                  LightenUIColor(GetThemeSurface(), 40), DarkenUIColor(GetThemeSurface(), 40));

    title_font = GetUITitleFontSize(GetLocaleText("meditation_title"), modal_w - side * 2);
    title_w = MeasureUIText(GetLocaleText("meditation_title"), title_font);
    DrawUIText(GetLocaleText("meditation_title"), modal_x + (modal_w - title_w) / 2,
                    modal_y + ScaleUIPx(16), title_font, GetThemeText());

    row_y = modal_y + ScaleUIPx(62);
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

    cancel_y = modal_y + modal_h - btn_h - ScaleUIPx(16);
    cancel_x = modal_x + (modal_w - cancel_w) / 2;
    if(DrawUIGenericButton(cancel_x, cancel_y, cancel_w, btn_h,
                              GetLocaleText("cancel_button"), UI_BUTTON_STYLE_SECONDARY,
                              0, &cancel_hover)) {
        app_close_modal(app);
    }
}

static void
draw_meditation_sound_controls(InbeApp *app)
{
    int title_h = GetUITitleBarHeight();

    if(app->show_session_volume_control) {
        if(DrawUIIconSliderPopup((UIIconSliderPopup){
               .id = 501,
               .x = view_width - ScaleUIPx(56),
               .y = (title_h - (ScaleUIPx(24) + ScaleUIPx(10) * 2)) / 2,
               .icon_size = ScaleUIPx(24),
               .icon_padding = ScaleUIPx(10),
               .icon = meditation_sound_icon_for_volume(app),
               .open = &app->volume_popup_active,
               .value = &app->sound_volume,
               .min = SETTINGS_VOLUME_MIN,
               .max = SETTINGS_VOLUME_MAX,
               .popup_width = ScaleUIPx(44),
               .popup_height = ScaleUIPx(200)
           })) {
            app->settings_dirty = 1;
            save_settings(app);
        }
    } else {
        app->volume_popup_active = 0;
    }
}

static void
draw_meditation_extend_controls(InbeApp *app, int center_x, int y)
{
    static const int add_seconds[] = {60, 5 * 60, 10 * 60};
    const char *labels[] = {"+1", "+5", "+10"};
    int gap = ScaleUIPx(8);
    int btn_h = ScaleUIPx(34);
    int btn_w = ScaleUIPx(58);
    int finish_w = ScaleUIPx(96);
    int total_w = btn_w * 3 + gap * 2;
    int x = center_x - total_w / 2;
    int hover = 0;

    if(app == NULL || !app->meditation.show_extend_controls)
        return;

    if(x < ScaleUIPx(12))
        x = ScaleUIPx(12);
    if(x + total_w > view_width - ScaleUIPx(12))
        x = view_width - ScaleUIPx(12) - total_w;

    for(int i = 0; i < 3; i++) {
        if(DrawUIGenericButton(x + i * (btn_w + gap), y, btn_w, btn_h, labels[i],
                                  UI_BUTTON_STYLE_SECONDARY, 0, &hover))
            meditation_extend_session(app, add_seconds[i]);
    }

    if(app->meditation.complete_waiting) {
        int finish_x = center_x - finish_w / 2;
        int finish_y = y + btn_h + ScaleUIPx(10);

        if(finish_x < ScaleUIPx(12))
            finish_x = ScaleUIPx(12);
        if(finish_x + finish_w > view_width - ScaleUIPx(12))
            finish_x = view_width - ScaleUIPx(12) - finish_w;

        if(DrawUIGenericButton(finish_x, finish_y, finish_w, btn_h,
                                  GetLocaleText("finish_button"), UI_BUTTON_STYLE_PRIMARY,
                                  0, &hover))
            meditation_finish(app);
    }
}

void
meditation_draw_screen(InbeApp *app, int center_x, int center_y)
{
    char time_text[32];
    int font = UI_TEXT_24;
    int max_w;
    int text_w;
    int title_h = GetUITitleBarHeight();

    if(app->show_session_return_button &&
       DrawUIReturnTitleBar(app->icons[UI_ICON_TYPE_RETURN], GetLocaleText("meditation_title"), title_h)) {
        meditation_request_exit(app);
        return;
    } else if(!app->show_session_return_button) {
        DrawUITitleBar(GetLocaleText("meditation_title"), title_h);
    }

    draw_meditation_sound_controls(app);

    if(app->modal.active && app->modal.type == UIModalConfirmExitSession) {
        int elapsed = meditation_elapsed_seconds(app);
        SessionExitModalResult result;

        result = app_draw_session_exit_modal(elapsed >= 60,
                                             GetLocaleText("meditation_save_elapsed_message"),
                                             GetLocaleText("meditation_under_minute_exit_message"));
        if(result == SessionExitModalCancel) {
            app_close_modal(app);
        } else if(result == SessionExitModalSave || result == SessionExitModalDiscard) {
            if(result == SessionExitModalSave)
                meditation_save_elapsed(app);
            meditation_exit_to_start(app);
        }
        return;
    }

    format_meditation_time(time_text, sizeof(time_text), app->meditation.remaining_seconds);
    max_w = view_width - ScaleUIPx(48);
    if(MeasureUIText(time_text, font) > max_w)
        font = UI_TEXT_16;
    text_w = MeasureUIText(time_text, font);
    DrawUIText(time_text, center_x - text_w / 2,
                    GetUIControlTextY(time_text, center_y - font, font * 2, font),
                    font, GetThemeText());

    draw_meditation_extend_controls(app, center_x, center_y + ScaleUIPx(42));
    if(app->meditation.complete_waiting)
        return;

    if(!app->backgrounded) {
        app->meditation.frame_ticks++;
        if(app->meditation.frame_ticks >= 60) {
            app->meditation.frame_ticks = 0;
            if(app->meditation.remaining_seconds > 0)
                app->meditation.remaining_seconds--;
            if(app->meditation.remaining_seconds <= 0)
                meditation_timer_complete(app);
        }
    }
}
