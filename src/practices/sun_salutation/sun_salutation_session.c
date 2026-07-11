#include "sun_salutation_practice.h"

#include "app.h"
#include "data.h"
#include "locale.h"
#include "theme.h"
#include "ui.h"
#include "practices/meditation/meditation_music.h"

#include <stdio.h>

extern int view_width;
extern int view_height;

static void
sun_salutation_exit_to_start(InbeApp *app)
{
    if(app == NULL)
        return;
    app->sun_salutation.step = 0;
    app->sun_salutation.repetition = 0;
    app->sun_salutation.step_ticks = 0;
    app->session_paused = 0;
    meditation_music_stop(app);
    app_close_modal(app);
    app_switch_screen(app, InbeScreenStart);
}

static void
sun_salutation_finish(InbeApp *app)
{
    int seconds = 1;

    if(app == NULL)
        return;
    if(data_save_session_path_for_activity(&seconds, 1, 0, EXERCISE_SUN_SALUTATION,
                                           NULL, 0)) {
        sync_habits_for_activity(app, EXERCISE_SUN_SALUTATION);
        app_auto_sync(app);
    }
    sun_salutation_exit_to_start(app);
}

void
sun_salutation_practice_start(InbeApp *app)
{
    if(app == NULL)
        return;
    mark_exercise_manual_seen(app, EXERCISE_SUN_SALUTATION);
    app->sun_salutation.step = 0;
    app->sun_salutation.repetition = 0;
    if(app->sun_salutation.repetitions < 2 || app->sun_salutation.repetitions > 12)
        app->sun_salutation.repetitions = SUN_SALUTATION_DEFAULT_REPETITIONS;
    if(app->sun_salutation.start_seconds < SUN_SALUTATION_SECONDS_MIN ||
       app->sun_salutation.start_seconds > SUN_SALUTATION_SECONDS_MAX)
        app->sun_salutation.start_seconds = SUN_SALUTATION_DEFAULT_START_SECONDS;
    if(app->sun_salutation.end_seconds < SUN_SALUTATION_SECONDS_MIN ||
       app->sun_salutation.end_seconds > SUN_SALUTATION_SECONDS_MAX)
        app->sun_salutation.end_seconds = SUN_SALUTATION_DEFAULT_END_SECONDS;
    if(app->sun_salutation.start_seconds < app->sun_salutation.end_seconds)
        app->sun_salutation.end_seconds = app->sun_salutation.start_seconds;
    app->sun_salutation.step_ticks = 0;
    app->session_paused = 0;
    app_close_modal(app);
    app_switch_screen(app, InbeScreenSunSalutation);
    meditation_music_start_session(app);
}

void
sun_salutation_request_exit(InbeApp *app)
{
    if(app == NULL)
        return;
    app_open_modal(app, UIModalConfirmExitSession);
}

static int
sun_salutation_step_seconds(InbeApp *app)
{
    int start_seconds;
    int end_seconds;
    int repetitions;
    int repetition;

    if(app == NULL)
        return SUN_SALUTATION_DEFAULT_START_SECONDS;
    start_seconds = app->sun_salutation.start_seconds;
    end_seconds = app->sun_salutation.end_seconds;
    repetitions = app->sun_salutation.repetitions;
    repetition = app->sun_salutation.repetition;
    if(start_seconds < SUN_SALUTATION_SECONDS_MIN ||
       start_seconds > SUN_SALUTATION_SECONDS_MAX)
        start_seconds = SUN_SALUTATION_DEFAULT_START_SECONDS;
    if(end_seconds < SUN_SALUTATION_SECONDS_MIN ||
       end_seconds > SUN_SALUTATION_SECONDS_MAX)
        end_seconds = SUN_SALUTATION_DEFAULT_END_SECONDS;
    if(start_seconds < end_seconds)
        end_seconds = start_seconds;
    if(repetitions < 2)
        return start_seconds;
    if(repetition < 0)
        repetition = 0;
    if(repetition >= repetitions)
        repetition = repetitions - 1;
    return start_seconds + ((end_seconds - start_seconds) * repetition) /
                               (repetitions - 1);
}

static void
sun_salutation_step_forward(InbeApp *app)
{
    if(app == NULL)
        return;
    app->sun_salutation.step_ticks = 0;
    if(app->sun_salutation.step >= SUN_SALUTATION_STEP_COUNT - 1) {
        if(app->sun_salutation.repetition >= app->sun_salutation.repetitions - 1)
            sun_salutation_finish(app);
        else {
            app->sun_salutation.repetition++;
            app->sun_salutation.step = 0;
        }
    } else {
        app->sun_salutation.step++;
    }
}

static void
sun_salutation_step_back(InbeApp *app)
{
    if(app == NULL)
        return;
    app->sun_salutation.step_ticks = 0;
    if(app->sun_salutation.step > 0) {
        app->sun_salutation.step--;
    } else if(app->sun_salutation.repetition > 0) {
        app->sun_salutation.repetition--;
        app->sun_salutation.step = SUN_SALUTATION_STEP_COUNT - 1;
    }
}

static void
sun_salutation_update_timer(InbeApp *app)
{
    int ticks_per_step;

    if(app == NULL || app->inbe.screen != InbeScreenSunSalutation ||
       app->session_paused)
        return;
    ticks_per_step = sun_salutation_step_seconds(app) * 60;
    if(ticks_per_step <= 0)
        ticks_per_step = SUN_SALUTATION_DEFAULT_START_SECONDS * 60;
    app->sun_salutation.step_ticks++;
    if(app->sun_salutation.step_ticks >= ticks_per_step)
        sun_salutation_step_forward(app);
}

static void
draw_pose_image(Texture2D texture, int top_y, int bottom_y)
{
    Rectangle src;
    Rectangle dst;
    float max_w;
    float max_h;
    float scale;
    float w;
    float h;

    if(texture.id == 0 || bottom_y <= top_y)
        return;

    max_w = (float)(view_width - ScaleUIPx(48));
    max_h = (float)(bottom_y - top_y);
    scale = max_w / (float)texture.width;
    if((float)texture.height * scale > max_h)
        scale = max_h / (float)texture.height;
    if(scale <= 0.0f)
        return;

    w = (float)texture.width * scale;
    h = (float)texture.height * scale;
    src = (Rectangle){0, 0, (float)texture.width, (float)texture.height};
    dst = (Rectangle){((float)view_width - w) * 0.5f,
                      (float)top_y + (max_h - h) * 0.5f,
                      w, h};
    DrawTexturePro(texture, src, dst, (Vector2){0}, 0.0f, WHITE);
}

void
sun_salutation_draw_screen(InbeApp *app, int center_x, int center_y)
{
    UIIconRowItem controls[3];
    UIIconRowResult row;
    Texture2D pose;
    const char *step_label;
    char step_title[32];
    int step;
    int repetition;
    int repetitions;
    int pose_index;
    int text_w;
    int title_h = GetUITitleBarHeight();
    int subtitle_h = ScaleUIPx(34);
    int content_top = title_h + subtitle_h + ScaleUIPx(8);
    int subtitle_font = GetUISmallFontSize();
    int subtitle_y;
    int image_bottom;
    int min_view_dim = view_width < view_height ? view_width : view_height;

    if(app == NULL)
        return;

    if(app->modal.active && app->modal.type == UIModalConfirmExitSession) {
        int modal_result = DrawUIModal(GetLocaleText("exit_session_title"),
                                         GetLocaleText("all_progress_lost_message"),
                                         GetLocaleText("cancel_button"),
                                         GetLocaleText("exit_button"));
        if(modal_result == 1)
            app_close_modal(app);
        else if(modal_result == 2)
            sun_salutation_exit_to_start(app);
        return;
    }

    step = app->sun_salutation.step;
    repetition = app->sun_salutation.repetition;
    repetitions = app->sun_salutation.repetitions;
    if(repetitions < 2 || repetitions > 12)
        repetitions = SUN_SALUTATION_DEFAULT_REPETITIONS;
    if(app->sun_salutation.start_seconds < SUN_SALUTATION_SECONDS_MIN ||
       app->sun_salutation.start_seconds > SUN_SALUTATION_SECONDS_MAX)
        app->sun_salutation.start_seconds = SUN_SALUTATION_DEFAULT_START_SECONDS;
    if(app->sun_salutation.end_seconds < SUN_SALUTATION_SECONDS_MIN ||
       app->sun_salutation.end_seconds > SUN_SALUTATION_SECONDS_MAX)
        app->sun_salutation.end_seconds = SUN_SALUTATION_DEFAULT_END_SECONDS;
    if(app->sun_salutation.start_seconds < app->sun_salutation.end_seconds)
        app->sun_salutation.end_seconds = app->sun_salutation.start_seconds;
    if(step < 0)
        step = 0;
    if(step >= SUN_SALUTATION_STEP_COUNT)
        step = SUN_SALUTATION_STEP_COUNT - 1;
    if(repetition < 0)
        repetition = 0;
    if(repetition >= repetitions)
        repetition = repetitions - 1;
    app->sun_salutation.step = step;
    app->sun_salutation.repetition = repetition;
    app->sun_salutation.repetitions = repetitions;
    pose_index = sun_salutation_step_pose_index(step);
    pose = app->sun_salutation.poses[pose_index];

    step_label = sun_salutation_step_label(step);
    FormatLocaleText(step_title, sizeof(step_title), "sun_salutation_session_step_title",
                     step + 1, SUN_SALUTATION_STEP_COUNT);
    if(app->show_session_return_button &&
       app_draw_close_title_bar(app, step_title, title_h)) {
        sun_salutation_request_exit(app);
        return;
    } else if(!app->show_session_return_button) {
        DrawUITitleBar(step_title, title_h);
    }
    text_w = MeasureUIText(step_label, subtitle_font);
    subtitle_y = title_h;
    DrawUIText(step_label, center_x - text_w / 2,
                    GetUIControlTextY(step_label, subtitle_y, subtitle_h, subtitle_font),
                    subtitle_font, GetThemeText());

    controls[0] = (UIIconRowItem){app->icons[UI_ICON_TYPE_BACKWARD],
                                       step <= 0 && repetition <= 0};
    controls[1] = (UIIconRowItem){app->session_paused ? app->icons[UI_ICON_TYPE_PLAY] :
                                                            app->icons[UI_ICON_TYPE_PAUSE],
                                       0};
    controls[2] = (UIIconRowItem){app->icons[UI_ICON_TYPE_FORWARD], 0};
    row = DrawUIBottomIconRow((UIBottomIconRow){
        .center_x = center_x,
        .view_width = view_width,
        .view_height = view_height,
        .count = 3,
        .items = controls,
        .icon_size = ScaleUIPx(24),
        .icon_padding = ScaleUIPx(10),
        .gap = ScaleUIPx(12),
        .side_margin = ScaleUIPx(24),
        .bottom_margin = ScaleUIPx(6),
        .max_button_width = min_view_dim / 6,
        .min_icon_size = ScaleUIPx(16),
        .min_icon_padding = ScaleUIPx(6),
        .min_gap = ScaleUIPx(8)
    });

    image_bottom = row.y - ScaleUIPx(18);
    draw_pose_image(pose, content_top, image_bottom);

    if(row.clicked_index == 0)
        sun_salutation_step_back(app);
    else if(row.clicked_index == 1)
        app->session_paused = !app->session_paused;
    else if(row.clicked_index == 2)
        sun_salutation_step_forward(app);

    sun_salutation_update_timer(app);

    (void)center_y;
}
