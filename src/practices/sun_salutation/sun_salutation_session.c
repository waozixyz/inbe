#include "sun_salutation_practice.h"

#include "app.h"
#include "data.h"
#include "flint_locale.h"
#include "flint_theme.h"
#include "flint_ui.h"

#include <stdio.h>

extern int view_width;
extern int view_height;

enum {
    SUN_SALUTATION_STEP_COUNT = 12
};

static const int g_step_pose_index[SUN_SALUTATION_STEP_COUNT] = {
    0, 1, 2, 3, 4, 5, 6, 7, 3, 2, 1, 0
};

static const char *g_step_text[SUN_SALUTATION_STEP_COUNT] = {
    "Step 1: Mountain pose",
    "Step 2: Upward salute",
    "Step 3: Standing forward fold",
    "Step 4: Half lift",
    "Step 5: Plank",
    "Step 6: Low plank",
    "Step 7: Upward-facing dog",
    "Step 8: Downward-facing dog",
    "Step 9: Half lift",
    "Step 10: Standing forward fold",
    "Step 11: Upward salute",
    "Step 12: Mountain pose"
};

static void
sun_salutation_exit_to_start(InbeApp *app)
{
    if(app == NULL)
        return;
    app->sun_salutation.step = 0;
    app->sun_salutation.repetition = 0;
    app->session_paused = 0;
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
        app->sun_salutation.repetitions = 3;
    app->session_paused = 0;
    app_close_modal(app);
    app_switch_screen(app, InbeScreenSunSalutation);
}

void
sun_salutation_request_exit(InbeApp *app)
{
    if(app == NULL)
        return;
    app_open_modal(app, UIModalConfirmExitSession);
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

    max_w = (float)(view_width - flint_px(48));
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
    FlintUIIconRowItem controls[2];
    FlintUIIconRowResult row;
    Texture2D pose;
    char step_label[32];
    int step;
    int repetition;
    int repetitions;
    int pose_index;
    int title_font = flint_ui_font();
    int body_font = flint_ui_font();
    int text_w;
    int return_hover = 0;
    int content_top = flint_px(72);
    const char *warning_text = "Work in progress";
    int warning_font = flint_ui_font_small();
    int warning_y;
    int text_y;
    int image_bottom;
    int min_view_dim = view_width < view_height ? view_width : view_height;

    if(app == NULL)
        return;

    if(ui_draw_icon_btn_padded(flint_px(12), flint_px(12), flint_px(24),
                               flint_px(10), app->icons[UI_ICON_TYPE_RETURN], &return_hover)) {
        sun_salutation_request_exit(app);
        return;
    }

    if(app->modal.active && app->modal.type == UIModalConfirmExitSession) {
        int modal_result = ui_draw_modal(locale_get("exit_session_title"),
                                         locale_get("all_progress_lost_message"),
                                         locale_get("cancel_button"),
                                         locale_get("exit_button"));
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
        repetitions = 3;
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
    pose_index = g_step_pose_index[step];
    pose = app->sun_salutation.poses[pose_index];

    snprintf(step_label, sizeof(step_label), "%d/%d - %d/%d",
             repetition + 1, repetitions, step + 1, SUN_SALUTATION_STEP_COUNT);
    text_w = flint_text_measure(step_label, title_font);
    flint_text_draw(step_label, center_x - text_w / 2,
                    flint_ui_text_y(step_label, flint_px(12), flint_px(44), title_font),
                    title_font, flint_theme_get_text());
    text_w = flint_text_measure(warning_text, warning_font);
    warning_y = flint_px(50);
    flint_text_draw(warning_text, center_x - text_w / 2,
                    flint_ui_text_y(warning_text, warning_y, flint_px(28), warning_font),
                    warning_font, flint_theme_get_text());

    controls[0] = (FlintUIIconRowItem){app->icons[UI_ICON_TYPE_BACKWARD],
                                       step <= 0 && repetition <= 0};
    controls[1] = (FlintUIIconRowItem){app->icons[UI_ICON_TYPE_FORWARD], 0};
    row = ui_draw_bottom_icon_row((FlintUIBottomIconRow){
        .center_x = center_x,
        .view_width = view_width,
        .view_height = view_height,
        .count = 2,
        .items = controls,
        .icon_size = flint_px(24),
        .icon_padding = flint_px(10),
        .gap = flint_px(16),
        .side_margin = flint_px(24),
        .bottom_margin = flint_px(6),
        .max_button_width = min_view_dim / 5,
        .min_icon_size = flint_px(16),
        .min_icon_padding = flint_px(6),
        .min_gap = flint_px(8)
    });

    text_y = row.y - flint_px(72);
    image_bottom = text_y - flint_px(18);
    draw_pose_image(pose, content_top, image_bottom);

    text_w = flint_text_measure(g_step_text[step], body_font);
    if(text_w > view_width - flint_px(48))
        body_font = flint_ui_font_small();
    text_w = flint_text_measure(g_step_text[step], body_font);
    flint_text_draw(g_step_text[step], center_x - text_w / 2,
                    flint_ui_text_y(g_step_text[step], text_y, flint_px(44), body_font),
                    body_font, flint_theme_get_text());

    if(row.clicked_index == 0) {
        if(step > 0) {
            app->sun_salutation.step--;
        } else if(repetition > 0) {
            app->sun_salutation.repetition--;
            app->sun_salutation.step = SUN_SALUTATION_STEP_COUNT - 1;
        }
    } else if(row.clicked_index == 1) {
        if(step >= SUN_SALUTATION_STEP_COUNT - 1) {
            if(repetition >= repetitions - 1)
                sun_salutation_finish(app);
            else {
                app->sun_salutation.repetition++;
                app->sun_salutation.step = 0;
            }
        } else {
            app->sun_salutation.step++;
        }
    }

    (void)center_y;
}
