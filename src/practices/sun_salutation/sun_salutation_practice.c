#include "sun_salutation_practice.h"

#include "app.h"
#include "app_settings.h"
#include "locale.h"
#include "practices/meditation/meditation_music.h"
#include "ui_text.h"
#include "theme.h"
#include "ui.h"

#include <stdio.h>

extern int view_width;
extern int view_height;

static const int g_sun_salutation_pose_sequence[SUN_SALUTATION_STEP_COUNT] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
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
    return GetLocaleText(g_sun_salutation_step_label_keys[step]);
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
    app->sun_salutation.figure =
        sun_salutation_clampi(app->sun_salutation.figure, 0,
                              SUN_SALUTATION_ACTIVE_FIGURE_COUNT - 1);
}

void
sun_salutation_practice_init(InbeApp *app)
{
    if(app == NULL)
        return;
    if(app->sun_salutation.banner.id == 0)
        app->sun_salutation.banner = app_load_asset_texture("practices/sunsalutation/banner.png");
    if(app->sun_salutation.pose_sheets[SUN_SALUTATION_FIGURE_MAN].id == 0)
        app->sun_salutation.pose_sheets[SUN_SALUTATION_FIGURE_MAN] =
            app_load_asset_texture("practices/sunsalutation/poses_man_sheet.png");
    if(app->sun_salutation.repetitions < 2 || app->sun_salutation.repetitions > 12)
        app->sun_salutation.repetitions = SUN_SALUTATION_DEFAULT_REPETITIONS;
    if(app->sun_salutation.start_seconds < SUN_SALUTATION_SECONDS_MIN ||
       app->sun_salutation.start_seconds > SUN_SALUTATION_SECONDS_MAX)
        app->sun_salutation.start_seconds = SUN_SALUTATION_DEFAULT_START_SECONDS;
    if(app->sun_salutation.end_seconds < SUN_SALUTATION_SECONDS_MIN ||
       app->sun_salutation.end_seconds > SUN_SALUTATION_SECONDS_MAX)
        app->sun_salutation.end_seconds = SUN_SALUTATION_DEFAULT_END_SECONDS;
    sun_salutation_normalize_settings(app);
}

void
sun_salutation_practice_destroy(InbeApp *app)
{
    if(app == NULL)
        return;
    app_unload_texture(app->sun_salutation.banner);
    app->sun_salutation.banner = (Texture2D){0};
    for(int i = 0; i < SUN_SALUTATION_FIGURE_COUNT; i++) {
        app_unload_texture(app->sun_salutation.pose_sheets[i]);
        app->sun_salutation.pose_sheets[i] = (Texture2D){0};
    }
}

static void
draw_preview_pose(Texture2D texture, int pose_index, int x, int y, int w, int h, Color tint)
{
    Rectangle src;
    Rectangle dst;
    float scale;
    float draw_w;
    float draw_h;

    if(texture.id == 0 || w <= 0 || h <= 0)
        return;
    if(pose_index < 0)
        pose_index = 0;
    if(pose_index >= SUN_SALUTATION_POSE_COUNT)
        pose_index = SUN_SALUTATION_POSE_COUNT - 1;
    scale = (float)w / (float)SUN_SALUTATION_POSE_FRAME_W;
    if((float)SUN_SALUTATION_POSE_FRAME_H * scale > (float)h)
        scale = (float)h / (float)SUN_SALUTATION_POSE_FRAME_H;
    draw_w = (float)SUN_SALUTATION_POSE_FRAME_W * scale;
    draw_h = (float)SUN_SALUTATION_POSE_FRAME_H * scale;
    src = (Rectangle){
        (float)((pose_index % SUN_SALUTATION_POSE_SHEET_COLS) *
                SUN_SALUTATION_POSE_FRAME_W),
        (float)((pose_index / SUN_SALUTATION_POSE_SHEET_COLS) *
                SUN_SALUTATION_POSE_FRAME_H),
        (float)SUN_SALUTATION_POSE_FRAME_W,
        (float)SUN_SALUTATION_POSE_FRAME_H
    };
    dst = (Rectangle){(float)x + ((float)w - draw_w) * 0.5f,
                      (float)y + ((float)h - draw_h) * 0.5f,
                      draw_w, draw_h};
    DrawTexturePro(texture, src, dst, (Vector2){0}, 0.0f, tint);
}

static int
sun_salutation_preview_height(int content_w)
{
    int thumb_w = (content_w - ScaleUIPx(22)) / 6;

    if(thumb_w < ScaleUIPx(34))
        thumb_w = ScaleUIPx(34);
    if(thumb_w > ScaleUIPx(56))
        thumb_w = ScaleUIPx(56);
    return thumb_w * 2 + ScaleUIPx(56);
}

static void
draw_sun_salutation_preview(InbeApp *app, int x, int y, int w)
{
    int label_font = GetUIFontSize();
    int small_font = GetUISmallFontSize();
    int label_w;
    int thumb_w = (w - ScaleUIPx(22)) / 6;
    int thumb_h;
    int gap = ScaleUIPx(4);
    int active_step;
    int preview_step_ticks;
    int row_y;
    Color muted = DarkenUIColor(GetThemeText(), 40);

    if(thumb_w < ScaleUIPx(34))
        thumb_w = ScaleUIPx(34);
    if(thumb_w > ScaleUIPx(56))
        thumb_w = ScaleUIPx(56);
    thumb_h = thumb_w;

    label_w = MeasureUIText(GetLocaleText("sun_salutation_tempo_preview_label"), label_font);
    DrawUIText(GetLocaleText("sun_salutation_tempo_preview_label"),
                    x + (w - label_w) / 2, y, label_font, GetThemeText());
    y += ScaleUIPx(28);

    preview_step_ticks = app->sun_salutation.start_seconds * 60;
    if(preview_step_ticks <= 0)
        preview_step_ticks = SUN_SALUTATION_DEFAULT_START_SECONDS * 60;
    active_step = (app->inbe.frame / preview_step_ticks) %
                  SUN_SALUTATION_STEP_COUNT;
    for(int i = 0; i < SUN_SALUTATION_STEP_COUNT; i++) {
        int col = i % 6;
        int row = i / 6;
        int px = x + col * (thumb_w + gap) + (w - (thumb_w * 6 + gap * 5)) / 2;
        int py = y + row * (thumb_h + ScaleUIPx(18));
        int pose_index = sun_salutation_step_pose_index(i);
        Color tint = i == active_step ? WHITE : Fade(WHITE, 0.62f);

        DrawRectangle(px, py, thumb_w, thumb_h, DarkenUIColor(GetThemeBackground(), 6));
        if(i == active_step)
            DrawRectangleLines(px, py, thumb_w, thumb_h, GetThemeButtonHover());
        draw_preview_pose(app->sun_salutation.pose_sheets[app->sun_salutation.figure], pose_index,
                          px, py, thumb_w, thumb_h, tint);
        if(i < SUN_SALUTATION_STEP_COUNT - 1) {
            int next_col = (i + 1) % 6;
            if(next_col != 0)
                DrawLine(px + thumb_w + ScaleUIPx(1), py + thumb_h / 2,
                         px + thumb_w + gap - ScaleUIPx(1), py + thumb_h / 2,
                         muted);
        }
    }

    row_y = y + thumb_h * 2 + ScaleUIPx(26);
    if(MeasureUIText(GetLocaleText("sun_salutation_tempo_preview_hint"),
                          small_font) <= w)
        DrawUIText(GetLocaleText("sun_salutation_tempo_preview_hint"), x, row_y,
                        small_font, muted);
}

static int
draw_sun_salutation_figure_selector(InbeApp *app, int x, int y, int w)
{
    int label_font = GetUIFontSize();
    int small_font = GetUISmallFontSize();
    int label_w;
    int row_y;
    int option_gap = ScaleUIPx(10);
    int option_w = (w - option_gap) / 2;
    int option_h = ScaleUIPx(118);
    int image_h = ScaleUIPx(72);
    int changed = 0;
    Vector2 mouse = GetMousePosition();

    if(app == NULL || SUN_SALUTATION_ACTIVE_FIGURE_COUNT <= 1)
        return 0;

    label_w = MeasureUIText(GetLocaleText("sun_salutation_figure_label"), label_font);
    DrawUIText(GetLocaleText("sun_salutation_figure_label"), x + (w - label_w) / 2,
               y, label_font, GetThemeText());
    row_y = y + ScaleUIPx(30);

    for(int i = 0; i < SUN_SALUTATION_ACTIVE_FIGURE_COUNT; i++) {
        Rectangle bounds = {
            (float)(x + i * (option_w + option_gap)),
            (float)row_y,
            (float)option_w,
            (float)option_h
        };
        Rectangle image_bounds = {
            bounds.x + ScaleUIPx(8),
            bounds.y + ScaleUIPx(8),
            bounds.width - ScaleUIPx(16),
            (float)image_h
        };
        int selected = app->sun_salutation.figure == i;
        int hover = CheckCollisionPointRec(mouse, bounds);
        Color bg = selected ? Fade(GetThemeButtonHover(), 0.18f)
                            : DarkenUIColor(GetThemeBackground(), hover ? 10 : 6);
        Color border = selected ? GetThemeButtonHover()
                                : DarkenUIColor(GetThemeText(), 45);
        const char *label;
        int text_w;
        int pose_index = sun_salutation_step_pose_index(1);

        if(i == SUN_SALUTATION_FIGURE_MAN)
            label = GetLocaleText("sun_salutation_figure_man");
        else
            label = GetLocaleText("sun_salutation_figure_woman");
        text_w = MeasureUIText(label, small_font);

        DrawRectangleRec(bounds, bg);
        DrawRectangleLinesEx(bounds, selected ? 2.0f : 1.0f, border);
        draw_preview_pose(app->sun_salutation.pose_sheets[i], pose_index,
                          (int)image_bounds.x, (int)image_bounds.y,
                          (int)image_bounds.width, (int)image_bounds.height,
                          WHITE);
        DrawUIText(label, (int)(bounds.x + (bounds.width - text_w) * 0.5f),
                   (int)(bounds.y + bounds.height - ScaleUIPx(28)),
                   small_font, GetThemeText());

        if(hover && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
           !UIInputCapturesClick(mouse) && !selected) {
            app->sun_salutation.figure = i;
            changed = 1;
        }
    }

    return changed;
}

typedef struct SunSalutationConfigScrollPageContext {
    InbeApp *app;
} SunSalutationConfigScrollPageContext;

static int
sun_salutation_config_content_height(int content_w, void *user_data)
{
    SunSalutationConfigScrollPageContext *ctx = user_data;
    int bottom_padding;

    if(ctx == NULL || ctx->app == NULL)
        return 0;

    bottom_padding = app_content_bottom_reserved(ctx->app) + ScaleUIPx(24);
    if(ctx->app->practice_config_tab == 1)
        return meditation_music_measure_practice_settings(ctx->app,
                                                          EXERCISE_SUN_SALUTATION,
                                                          content_w, 1, 1) +
               bottom_padding;
    return (SUN_SALUTATION_ACTIVE_FIGURE_COUNT > 1 ? ScaleUIPx(148) + ScaleUIPx(16) : 0) +
           sun_salutation_preview_height(content_w) + ScaleUIPx(16) +
           ScaleUIPx(74) * 3 + bottom_padding;
}

void
sun_salutation_config_screen_draw(InbeApp *app)
{
    int title_h = app_content_top_reserved(app);
    int config_tab_h = ScaleUIPx(40);
    int config_tab_gap = ScaleUIPx(14);
    int scroll_y;
    int scroll_h;
    int repetitions;
    int start_seconds;
    int end_seconds;
    int clicked_config_tab;
    const char *config_tabs[] = {
        GetLocaleText("practice_config_title"),
        GetLocaleText("practice_music_title"),
    };

    if(app == NULL)
        return;
    sun_salutation_normalize_settings(app);
    if(app->practice_config_tab < 0 || app->practice_config_tab > 1)
        app->practice_config_tab = 0;
    if(app->modal.active && app->modal.type == UIModalPracticeConfig) {
        title_h = GetUITitleBarHeight();
        if(app_draw_close_title_bar(app, GetLocaleText("practice_config_title"), title_h)) {
            app_close_modal(app);
            return;
        }
    } else if(app_draw_close_title_bar(app,
                                        GetLocaleText("practice_config_title"),
                                        title_h)) {
        if(app->settings_dirty)
            save_settings(app);
        app->settings_scroll = 0;
        app->practice_tab = PRACTICE_TAB_PLAY;
        app_switch_screen(app, InbeScreenStart);
        return;
    }

    {
        UISubtab tabs[2] = {
            {.label = config_tabs[0], .disabled = 0},
            {.label = config_tabs[1], .disabled = 0},
        };
        clicked_config_tab = DrawUISubtabBar((UISubtabBar){
            .bounds = {0, (float)title_h, (float)view_width, (float)config_tab_h},
            .tabs = tabs,
            .count = 2,
            .selected_index = app->practice_config_tab
        });
    }
    if(clicked_config_tab >= 0 && clicked_config_tab != app->practice_config_tab) {
        AppRoute route = app_current_route(app);
        route.practice_config_tab = clicked_config_tab;
        app->settings_scroll = 0;
        meditation_music_unload(app);
        app_switch_route(app, route);
    }

    scroll_y = title_h + config_tab_h + config_tab_gap;
    scroll_h = view_height - scroll_y - app_content_bottom_reserved(app);
    if(scroll_h < 0)
        scroll_h = 0;

    {
        SunSalutationConfigScrollPageContext page_ctx = {app};
        UIScrollPage page = BeginUIScrollPage((UIScrollPageSpec){
            .y = scroll_y,
            .height = scroll_h,
            .max_content_width = ScaleUIPx(CONTENT_MAX_W),
            .min_content_width = ScaleUIPx(320),
            .scroll_offset = &app->settings_scroll,
            .content_height = sun_salutation_config_content_height,
            .user_data = &page_ctx
        });
        int y = page.content_y;

        if(app->practice_config_tab == 0) {
            if(SUN_SALUTATION_ACTIVE_FIGURE_COUNT > 1) {
                if(draw_sun_salutation_figure_selector(app, page.content_x, y, page.content_w)) {
                    save_settings(app);
                }
                y += ScaleUIPx(148) + ScaleUIPx(16);
            }

            draw_sun_salutation_preview(app, page.content_x, y, page.content_w);
            y += sun_salutation_preview_height(page.content_w) + ScaleUIPx(16);

            repetitions = app->sun_salutation.repetitions;
            if(DrawUISlider(610, page.content_x, y, page.content_w,
                              GetLocaleText("sun_salutation_repetitions_label"),
                              2, 12, &repetitions, "x")) {
                app->sun_salutation.repetitions = repetitions;
                save_settings(app);
            }
            y += ScaleUIPx(74);

            start_seconds = app->sun_salutation.start_seconds;
            if(DrawUISlider(611, page.content_x, y, page.content_w,
                              GetLocaleText("sun_salutation_start_speed_label"),
                              SUN_SALUTATION_SECONDS_MIN, SUN_SALUTATION_SECONDS_MAX,
                              &start_seconds, "s")) {
                app->sun_salutation.start_seconds = start_seconds;
                sun_salutation_normalize_settings(app);
                save_settings(app);
            }
            y += ScaleUIPx(74);

            end_seconds = app->sun_salutation.end_seconds;
            if(DrawUISlider(612, page.content_x, y, page.content_w,
                              GetLocaleText("sun_salutation_end_speed_label"),
                              SUN_SALUTATION_SECONDS_MIN, app->sun_salutation.start_seconds,
                              &end_seconds, "s")) {
                app->sun_salutation.end_seconds = end_seconds;
                sun_salutation_normalize_settings(app);
                save_settings(app);
            }
        } else {
            meditation_music_draw_practice_settings(app, EXERCISE_SUN_SALUTATION,
                                                    page.content_x, page.content_w,
                                                    &y, 1, 1);
        }
        EndUIScrollPage(page);
    }
    if(app->practice_config_tab == 1)
        meditation_music_draw_dropdown_menu(app);
}
