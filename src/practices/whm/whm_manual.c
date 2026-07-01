#include "whm_practice.h"
#include "app.h"
#include "screens/manual_screen.h"
#include "whm_session.h"
#include "flint_locale.h"
#include "flint_theme.h"
#include "flint_clip.h"
#include "flint_ui.h"
#include "flint_theme_meta.h"
#include "raylib.h"
#include <stdio.h>

#define CONTENT_MAX_W 800  /* Responsive width for tutorial text reflow */

extern int view_width;
extern int view_height;

/* Tutorial text content ordered by step */
static const char *const TUTORIAL_KEYS[] = {
    "tutorial_step_intro",
    "tutorial_step_method",
    "tutorial_step_breathe",
    "tutorial_step_exhale_hold",
    "tutorial_step_inhale_hold"
};

#define TUTORIAL_STEPS_COUNT (sizeof(TUTORIAL_KEYS) / sizeof(TUTORIAL_KEYS[0]))
#define TUTORIAL_LINE_SPACING flint_px(8)  /* Normal readable line spacing */

static Color
manual_text_color_for_background(Color background)
{
    int luma = background.r * 299 + background.g * 587 + background.b * 114;

    return luma >= 128000 ? BLACK : WHITE;
}

static void
draw_tutorial_hold_preview(InbeApp *app, int center_x, int center_y, int radius)
{
    int seconds = (app->inbe.frame / 60) % 60;
    char text[CountSize];
    int font = FLINT_TEXT_16;
    int thickness = flint_px(5);
    Color text_color = flint_theme_get_text();

    count_from_int(text, seconds);
    if(thickness < 3)
        thickness = 3;

    if(app->hold_display_mode == HOLD_DISPLAY_CIRCLE) {
        float sweep = 360.0f * (float)((app->inbe.frame % (60 * 60))) / (float)(60 * 60);

        DrawRing((Vector2){center_x, center_y},
                 (float)(radius + flint_px(8) - thickness / 2),
                 (float)(radius + flint_px(8) + thickness / 2),
                 -90.0f, -90.0f + sweep, 96, flint_theme_get_text());
        text_color = manual_text_color_for_background(flint_theme_get_bg());
    } else {
        DrawCircle(center_x, center_y, radius, flint_theme_get_circle());
        DrawCircleLines(center_x, center_y, radius, flint_theme_get_text());
        text_color = manual_text_color_for_background(flint_theme_get_circle());
    }

    flint_ui_draw_text_centered(text, center_x, center_y, font, text_color);
}

static FlintUIParagraph
tutorial_paragraph(InbeApp *app, int step, int content_w, int body_font)
{
    int has_icon = step == 1;
    return (FlintUIParagraph){
        .text = locale_get(TUTORIAL_KEYS[step]),
        .icon = has_icon ? app->icons[UI_ICON_TYPE_WRENCH] : (Texture2D){0},
        .icon_type = has_icon ? UI_ICON_TYPE_WRENCH : UI_ICON_TYPE_NONE,
        .icon_size = has_icon ? flint_px(14) : 0,
        .width = content_w,
        .font = body_font,
        .line_gap = TUTORIAL_LINE_SPACING,
    };
}

static int
tutorial_paragraph_height(InbeApp *app, int step, int content_w, int body_font)
{
    return flint_ui_paragraph_height(tutorial_paragraph(app, step, content_w, body_font));
}

static void
draw_tutorial_paragraph(InbeApp *app, int step, int content_x, int *y, int content_w, int body_font)
{
    flint_ui_paragraph_draw(tutorial_paragraph(app, step, content_w, body_font), content_x, y);
}

static int
manual_tutorial_content_height(InbeApp *app, int step, int content_w, int body_font)
{
    int actual_content_h = 0;

    if(step == 0) {
        actual_content_h += flint_px(170) + flint_px(22) +
                            tutorial_paragraph_height(app, 0, content_w, body_font);
    } else if(step == 1) {
        actual_content_h += tutorial_paragraph_height(app, 1, content_w, body_font);
    } else if(step == 2) {
        int preview_span;
        int preview_rmax;
        int slider_h = flint_px(40);

        actual_content_h += tutorial_paragraph_height(app, 2, content_w, body_font) + flint_px(20);
        preview_span = (content_w < flint_px(132)) ? content_w : flint_px(132);
        preview_rmax = preview_span / 2;
        if(preview_rmax < flint_px(60)) preview_rmax = flint_px(60);
        if(preview_rmax > flint_px(120)) preview_rmax = flint_px(120);
        actual_content_h += flint_px(40) + (int)((float)preview_rmax * 0.72f) + flint_px(14);
        actual_content_h += slider_h + flint_px(8);
    } else if(step == 3) {
        int hold_preview_radius = flint_px(54);
        int hold_preview_extent = hold_preview_radius + flint_px(8) + flint_px(5);

        actual_content_h += hold_preview_extent * 2 + flint_px(24) +
                            flint_px(42) + flint_px(18);
        actual_content_h += tutorial_paragraph_height(app, 3, content_w, body_font);
    } else {
        actual_content_h += flint_px(80) +
                            tutorial_paragraph_height(app, 4, content_w, body_font);
    }

    return actual_content_h;
}

typedef struct WhmManualScrollPageContext {
    InbeApp *app;
    int step;
    int body_font;
    int top_padding;
} WhmManualScrollPageContext;

static int
whm_manual_scroll_page_content_height(int content_w, void *user_data)
{
    WhmManualScrollPageContext *ctx = user_data;

    return manual_tutorial_content_height(ctx->app, ctx->step,
                                          content_w, ctx->body_font) +
           ctx->top_padding;
}

void
whm_manual_close(InbeApp *app, int mark_seen)
{
    if(mark_seen)
        mark_exercise_manual_seen(app, EXERCISE_WIM_HOF);
    if(app != NULL && app->modal.active && app->modal.type == UIModalPracticeManual) {
        app_close_modal(app);
        return;
    }
    app->tutorial_step = 0;
    app->manual_scroll = 0;
    app->practice_tab = PRACTICE_TAB_PLAY;
    app_switch_screen(app, InbeScreenStart);
}

static void
manual_screen_start_exercise(InbeApp *app)
{
    mark_exercise_manual_seen(app, EXERCISE_WIM_HOF);
    app->tutorial_step = 0;
    app->manual_scroll = 0;
    session_start(app);
}

void
whm_manual_draw(InbeApp *app)
{
    int integrated = app->inbe.screen == InbeScreenStart &&
                     app->modal.type != UIModalPracticeManual;
    int title_h = integrated ? app_content_top_reserved(app) : flint_ui_title_bar_height();
    int nav_h = manual_screen_guide_nav_height();
    int bottom_reserved = integrated ? app_content_bottom_reserved(app) : 0;
    int nav_y = view_height - bottom_reserved - nav_h;
    int content_y = title_h;
    int viewport_h = nav_y - content_y;
    int body_font = flint_ui_font();
    int footer_content_pad = flint_ui_font() / 2;
    int content_area_h = viewport_h - footer_content_pad;  /* For scissor mode and scroll calculations */
    const char *title = locale_get("tutorial_title");
    int step = app->tutorial_step;

    step = manual_screen_guide_update_page(app, (int)TUTORIAL_STEPS_COUNT,
                                           manual_screen_start_exercise,
                                           whm_manual_close);

    switch(step) {
    case 1: title = locale_get("tutorial_method_title"); break;
    case 2: title = locale_get("tutorial_step1_title"); break;
    case 3: title = locale_get("tutorial_step2_title"); break;
    case 4: title = locale_get("tutorial_step3_title"); break;
    default: break;
    }

    if(!integrated &&
       flint_ui_return_title_bar(app->icons[UI_ICON_TYPE_RETURN], title, title_h))
        whm_manual_close(app, 0);

    {
        int top_padding = flint_px(16);
        int responsive_max_w = (int)(view_width * 0.96f);
        int max_content_w = flint_px(CONTENT_MAX_W);
        WhmManualScrollPageContext page_ctx;
        FlintUIScrollPage page;

        if(responsive_max_w > max_content_w)
            responsive_max_w = max_content_w;
        if(responsive_max_w < flint_px(280))
            responsive_max_w = flint_px(280);

        page_ctx = (WhmManualScrollPageContext){app, step, body_font, top_padding};
        page = ui_scroll_page_begin((FlintUIScrollPageSpec){
            .y = content_y,
            .height = content_area_h,
            .max_content_width = responsive_max_w,
            .min_content_width = flint_px(280),
            .scroll_offset = &app->manual_scroll,
            .content_height = whm_manual_scroll_page_content_height,
            .user_data = &page_ctx
        });

        int y = page.content_y + top_padding;
        if(step == 0) {
            int img_h = flint_px(170);
            ui_draw_tutorial_image(app->whm.image_1, "practices/whm/1.jpg",
                                   page.content_x, y, page.content_w, img_h);
            y += img_h + flint_px(22);

            draw_tutorial_paragraph(app, 0, page.content_x, &y, page.content_w, body_font);
        } else if(step == 1) {
            draw_tutorial_paragraph(app, 1, page.content_x, &y, page.content_w, body_font);
        } else if(step == 2) {
            int speed = app->inbe.speed_level;
            draw_tutorial_paragraph(app, 2, page.content_x, &y, page.content_w, body_font);
            y += flint_px(20);  /* Increased spacing between text and circle */

            update_preview_bounds(&app->settings_preview, page.content_w, flint_px(132));
            apply_settings(&app->settings_preview, speed, app->inbe.max_rounds,
                           int_from_count(app->inbe.maxbreaths), app->inbe.pause_seconds);
            app->settings_preview.progressive_speed = 0;
            inbestep(&app->settings_preview);
            if(app->settings_preview.phase != InbePhaseBreathe) {
                reset_settings_preview(app);
                update_preview_bounds(&app->settings_preview, page.content_w, flint_px(132));
                apply_settings(&app->settings_preview, speed, app->inbe.max_rounds,
                               int_from_count(app->inbe.maxbreaths), app->inbe.pause_seconds);
                app->settings_preview.progressive_speed = 0;
            }
            draw_preview_inbe(&app->settings_preview,
                              page.content_x + page.content_w / 2, y + flint_px(40));
            y += (int)(app->settings_preview.rmax * 0.72f) + flint_px(54);

            if(ui_draw_slider(10, page.content_x, y, page.content_w, locale_get("speed_label"), SETTINGS_SPEED_MIN,
                           SETTINGS_SPEED_MAX, &speed, "")) {
                apply_settings(&app->inbe, speed, app->inbe.max_rounds,
                               int_from_count(app->inbe.maxbreaths), app->inbe.pause_seconds);
                apply_settings(&app->settings_preview, speed, app->inbe.max_rounds,
                               int_from_count(app->inbe.maxbreaths), app->inbe.pause_seconds);
                app->settings_preview.progressive_speed = 0;
                app->settings_dirty = 1;
            }
        } else if(step == 3) {
            int hold_preview_radius = flint_px(54);
            int hold_preview_extent = hold_preview_radius + flint_px(8) + flint_px(5);
            int breath_button_hover = 0;
            int center_x = page.content_x + page.content_w / 2;
            y += hold_preview_extent;
            draw_tutorial_hold_preview(app, center_x, y, hold_preview_radius);
            y += hold_preview_extent + flint_px(24);
            ui_draw_text_btn(center_x, y, locale_get("breath_button"), &breath_button_hover);
            y += flint_px(42) + flint_px(18);
            draw_tutorial_paragraph(app, 3, page.content_x, &y, page.content_w, body_font);
        } else {
            draw_tutorial_paragraph(app, 4, page.content_x, &y, page.content_w, body_font);
        }
        ui_scroll_page_end(page);
    }

    manual_screen_guide_draw_nav(app, (ManualGuideNav){
        .page = step,
        .page_count = (int)TUTORIAL_STEPS_COUNT,
        .y = nav_y,
        .h = nav_h,
        .show_left_on_first = 1,
        .start = manual_screen_start_exercise,
        .close = whm_manual_close
    });

    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        app->settings_drag_slider = 0;
        if(app->settings_dirty)
            save_settings(app);
    }
}
