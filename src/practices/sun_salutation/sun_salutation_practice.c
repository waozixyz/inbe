#include "sun_salutation_practice.h"

#include "app.h"
#include "app_settings.h"
#include "flint_locale.h"
#include "flint_text.h"
#include "flint_theme.h"
#include "flint_ui.h"

#include <stdio.h>

extern int view_width;

static const int g_sun_salutation_pose_sequence[SUN_SALUTATION_STEP_COUNT] = {
    0, 1, 2, 3, 4, 5, 6, 7, 3, 2, 1, 0
};

static const char *g_sun_salutation_step_label_keys[SUN_SALUTATION_STEP_COUNT] = {
    "sun_salutation_step_1",
    "sun_salutation_step_2",
    "sun_salutation_step_3",
    "sun_salutation_step_4",
    "sun_salutation_step_5",
    "sun_salutation_step_6",
    "sun_salutation_step_7",
    "sun_salutation_step_8",
    "sun_salutation_step_9",
    "sun_salutation_step_10",
    "sun_salutation_step_11",
    "sun_salutation_step_12"
};

static int
sun_salutation_clampi(int value, int min_value, int max_value)
{
    if(value < min_value)
        return min_value;
    if(value > max_value)
        return max_value;
    return value;
}

int
sun_salutation_step_pose_index(int step)
{
    step = sun_salutation_clampi(step, 0, SUN_SALUTATION_STEP_COUNT - 1);
    return g_sun_salutation_pose_sequence[step];
}

const char *
sun_salutation_step_label(int step)
{
    step = sun_salutation_clampi(step, 0, SUN_SALUTATION_STEP_COUNT - 1);
    return locale_get(g_sun_salutation_step_label_keys[step]);
}

static void
sun_salutation_normalize_settings(InbeApp *app)
{
    if(app == NULL)
        return;
    app->sun_salutation.repetitions =
        sun_salutation_clampi(app->sun_salutation.repetitions,
                              2, 12);
    app->sun_salutation.start_seconds =
        sun_salutation_clampi(app->sun_salutation.start_seconds,
                              SUN_SALUTATION_SECONDS_MIN,
                              SUN_SALUTATION_SECONDS_MAX);
    app->sun_salutation.end_seconds =
        sun_salutation_clampi(app->sun_salutation.end_seconds,
                              SUN_SALUTATION_SECONDS_MIN,
                              SUN_SALUTATION_SECONDS_MAX);
    if(app->sun_salutation.start_seconds < app->sun_salutation.end_seconds)
        app->sun_salutation.end_seconds = app->sun_salutation.start_seconds;
}

void
sun_salutation_practice_init(InbeApp *app)
{
    char path[64];

    if(app == NULL)
        return;
    if(app->sun_salutation.repetitions < 2 || app->sun_salutation.repetitions > 12)
        app->sun_salutation.repetitions = SUN_SALUTATION_DEFAULT_REPETITIONS;
    if(app->sun_salutation.start_seconds < SUN_SALUTATION_SECONDS_MIN ||
       app->sun_salutation.start_seconds > SUN_SALUTATION_SECONDS_MAX)
        app->sun_salutation.start_seconds = SUN_SALUTATION_DEFAULT_START_SECONDS;
    if(app->sun_salutation.end_seconds < SUN_SALUTATION_SECONDS_MIN ||
       app->sun_salutation.end_seconds > SUN_SALUTATION_SECONDS_MAX)
        app->sun_salutation.end_seconds = SUN_SALUTATION_DEFAULT_END_SECONDS;
    sun_salutation_normalize_settings(app);
    for(int i = 0; i < SUN_SALUTATION_POSE_COUNT; i++) {
        if(app->sun_salutation.poses[i].id != 0)
            continue;
        snprintf(path, sizeof(path), "practices/sunsalutation/pos_%04d.png", i + 1);
        app->sun_salutation.poses[i] = app_load_asset_texture(path);
    }
}

void
sun_salutation_practice_destroy(InbeApp *app)
{
    if(app == NULL)
        return;
    for(int i = 0; i < SUN_SALUTATION_POSE_COUNT; i++) {
        app_unload_texture(app->sun_salutation.poses[i]);
        app->sun_salutation.poses[i] = (Texture2D){0};
    }
}

static void
draw_preview_pose(Texture2D texture, int x, int y, int w, int h, Color tint)
{
    Rectangle src;
    Rectangle dst;
    float scale;
    float draw_w;
    float draw_h;

    if(texture.id == 0 || w <= 0 || h <= 0)
        return;
    scale = (float)w / (float)texture.width;
    if((float)texture.height * scale > (float)h)
        scale = (float)h / (float)texture.height;
    draw_w = (float)texture.width * scale;
    draw_h = (float)texture.height * scale;
    src = (Rectangle){0, 0, (float)texture.width, (float)texture.height};
    dst = (Rectangle){(float)x + ((float)w - draw_w) * 0.5f,
                      (float)y + ((float)h - draw_h) * 0.5f,
                      draw_w, draw_h};
    DrawTexturePro(texture, src, dst, (Vector2){0}, 0.0f, tint);
}

static int
sun_salutation_preview_height(int content_w)
{
    int thumb_w = (content_w - flint_px(22)) / 6;

    if(thumb_w < flint_px(34))
        thumb_w = flint_px(34);
    if(thumb_w > flint_px(56))
        thumb_w = flint_px(56);
    return thumb_w * 2 + flint_px(56);
}

static void
draw_sun_salutation_preview(InbeApp *app, int x, int y, int w)
{
    int label_font = flint_ui_font();
    int small_font = flint_ui_font_small();
    int label_w;
    int thumb_w = (w - flint_px(22)) / 6;
    int thumb_h;
    int gap = flint_px(4);
    int active_step;
    int preview_step_ticks;
    int row_y;
    Color muted = flint_darken(flint_theme_get_text(), 40);

    if(thumb_w < flint_px(34))
        thumb_w = flint_px(34);
    if(thumb_w > flint_px(56))
        thumb_w = flint_px(56);
    thumb_h = thumb_w;

    label_w = flint_text_measure(locale_get("sun_salutation_tempo_preview_label"), label_font);
    flint_text_draw(locale_get("sun_salutation_tempo_preview_label"),
                    x + (w - label_w) / 2, y, label_font, flint_theme_get_text());
    y += flint_px(28);

    preview_step_ticks = app->sun_salutation.start_seconds * 60;
    if(preview_step_ticks <= 0)
        preview_step_ticks = SUN_SALUTATION_DEFAULT_START_SECONDS * 60;
    active_step = (app->inbe.frame / preview_step_ticks) %
                  SUN_SALUTATION_STEP_COUNT;
    for(int i = 0; i < SUN_SALUTATION_STEP_COUNT; i++) {
        int col = i % 6;
        int row = i / 6;
        int px = x + col * (thumb_w + gap) + (w - (thumb_w * 6 + gap * 5)) / 2;
        int py = y + row * (thumb_h + flint_px(18));
        int pose_index = sun_salutation_step_pose_index(i);
        Color tint = i == active_step ? WHITE : Fade(WHITE, 0.62f);

        DrawRectangle(px, py, thumb_w, thumb_h, flint_darken(flint_theme_get_bg(), 6));
        if(i == active_step)
            DrawRectangleLines(px, py, thumb_w, thumb_h, flint_theme_get_button_hover());
        draw_preview_pose(app->sun_salutation.poses[pose_index], px, py, thumb_w, thumb_h, tint);
        if(i < SUN_SALUTATION_STEP_COUNT - 1) {
            int next_col = (i + 1) % 6;
            if(next_col != 0)
                DrawLine(px + thumb_w + flint_px(1), py + thumb_h / 2,
                         px + thumb_w + gap - flint_px(1), py + thumb_h / 2,
                         muted);
        }
    }

    row_y = y + thumb_h * 2 + flint_px(26);
    if(flint_text_measure(locale_get("sun_salutation_tempo_preview_hint"),
                          small_font) <= w)
        flint_text_draw(locale_get("sun_salutation_tempo_preview_hint"), x, row_y,
                        small_font, muted);
}

void
sun_salutation_config_screen_draw(InbeApp *app)
{
    int content_x;
    int content_w;
    int y;
    int repetitions;
    int start_seconds;
    int end_seconds;

    if(app == NULL)
        return;
    sun_salutation_normalize_settings(app);

    flint_centered_column(CONTENT_MAX_W, CONTENT_SIDE_PAD, &content_x, &content_w);
    y = app_content_top_reserved(app) + flint_px(20);

    draw_sun_salutation_preview(app, content_x, y, content_w);
    y += sun_salutation_preview_height(content_w) + flint_px(16);

    repetitions = app->sun_salutation.repetitions;
    if(ui_draw_slider(610, content_x, y, content_w,
                      locale_get("sun_salutation_repetitions_label"),
                      2, 12, &repetitions, "x")) {
        app->sun_salutation.repetitions = repetitions;
        save_settings(app);
    }
    y += flint_px(74);

    start_seconds = app->sun_salutation.start_seconds;
    if(ui_draw_slider(611, content_x, y, content_w,
                      locale_get("sun_salutation_start_speed_label"),
                      SUN_SALUTATION_SECONDS_MIN, SUN_SALUTATION_SECONDS_MAX,
                      &start_seconds, "s")) {
        app->sun_salutation.start_seconds = start_seconds;
        sun_salutation_normalize_settings(app);
        save_settings(app);
    }
    y += flint_px(74);

    end_seconds = app->sun_salutation.end_seconds;
    if(ui_draw_slider(612, content_x, y, content_w,
                      locale_get("sun_salutation_end_speed_label"),
                      SUN_SALUTATION_SECONDS_MIN, app->sun_salutation.start_seconds,
                      &end_seconds, "s")) {
        app->sun_salutation.end_seconds = end_seconds;
        sun_salutation_normalize_settings(app);
        save_settings(app);
    }
}
