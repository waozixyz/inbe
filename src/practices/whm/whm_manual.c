#include "whm_practice.h"
#include "app.h"
#include "screens/manual_screen.h"
#include "screens/practice_screen.h"
#include "whm_session.h"
#include "locale.h"
#include "theme.h"
#include "ui_clip.h"
#include "ui.h"
#include "theme_meta.h"
#include "flint.h"
#include <stdio.h>

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
    int font = UI_TEXT_16;
    int thickness = ScaleUIPx(5);
    Color text_color = GetThemeText();

    count_from_int(text, seconds);
    if(thickness < 3)
        thickness = 3;

    if(app->hold_display_mode == HOLD_DISPLAY_CIRCLE) {
        float sweep = 360.0f * (float)((app->inbe.frame % (60 * 60))) / (float)(60 * 60);

        DrawRing((Vector2){center_x, center_y},
                 (float)(radius + ScaleUIPx(8) - thickness / 2),
                 (float)(radius + ScaleUIPx(8) + thickness / 2),
                 -90.0f, -90.0f + sweep, 96, GetThemeText());
        text_color = manual_text_color_for_background(GetThemeBackground());
    } else {
        DrawCircle(center_x, center_y, radius, GetThemeCircle());
        DrawCircleLines(center_x, center_y, radius, GetThemeText());
        text_color = manual_text_color_for_background(GetThemeCircle());
    }

    DrawCenteredUIControlText(text, center_x, center_y, font, text_color);
}

static UIParagraph
tutorial_paragraph(InbeApp *app, int step, int content_w)
{
    int has_icon = step == 1;
    UIParagraph paragraph = manual_screen_tutorial_paragraph(
        GetLocaleText(TUTORIAL_KEYS[step]), content_w);

    paragraph.icon = has_icon ? app->icons[UI_ICON_TYPE_WRENCH] : (Texture2D){0};
    paragraph.icon_type = has_icon ? UI_ICON_TYPE_WRENCH : UI_ICON_TYPE_NONE;
    paragraph.icon_size = has_icon ? ScaleUIPx(14) : 0;
    return paragraph;
}

static int
tutorial_paragraph_height(InbeApp *app, int step, int content_w)
{
    return GetUIParagraphHeight(tutorial_paragraph(app, step, content_w));
}

static void
draw_tutorial_paragraph(InbeApp *app, int step, int content_x, int *y, int content_w)
{
    DrawUIParagraph(tutorial_paragraph(app, step, content_w), content_x, y);
}

static int
manual_tutorial_content_height(InbeApp *app, int step, int content_w)
{
    int actual_content_h = 0;

    if(step == 0) {
        actual_content_h += ScaleUIPx(170) + ScaleUIPx(22) +
                            tutorial_paragraph_height(app, 0, content_w);
    } else if(step == 1) {
        actual_content_h += tutorial_paragraph_height(app, 1, content_w);
    } else if(step == 2) {
        int preview_span;
        int preview_rmax;
        int slider_h = ScaleUIPx(40);

        actual_content_h += tutorial_paragraph_height(app, 2, content_w) + ScaleUIPx(20);
        preview_span = (content_w < ScaleUIPx(132)) ? content_w : ScaleUIPx(132);
        preview_rmax = preview_span / 2;
        if(preview_rmax < ScaleUIPx(60)) preview_rmax = ScaleUIPx(60);
        if(preview_rmax > ScaleUIPx(120)) preview_rmax = ScaleUIPx(120);
        actual_content_h += ScaleUIPx(40) + (int)((float)preview_rmax * 0.72f) + ScaleUIPx(14);
        actual_content_h += slider_h + ScaleUIPx(8);
    } else if(step == 3) {
        int hold_preview_radius = ScaleUIPx(54);
        int hold_preview_extent = hold_preview_radius + ScaleUIPx(8) + ScaleUIPx(5);

        actual_content_h += hold_preview_extent * 2 + ScaleUIPx(24) +
                            ScaleUIPx(42) + ScaleUIPx(18);
        actual_content_h += tutorial_paragraph_height(app, 3, content_w);
    } else {
        actual_content_h += ScaleUIPx(170) + ScaleUIPx(22) +
                            tutorial_paragraph_height(app, 4, content_w);
    }

    return actual_content_h;
}

typedef struct WhmManualScrollPageContext {
    InbeApp *app;
    int step;
} WhmManualScrollPageContext;

static int
whm_manual_scroll_page_content_height(int content_w, void *user_data)
{
    WhmManualScrollPageContext *ctx = user_data;

    return manual_tutorial_content_height(ctx->app, ctx->step, content_w);
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
    PracticeManualLayout layout;
    int footer_content_pad = GetUIFontSize() / 2;
    int step = app->tutorial_step;

    step = manual_screen_guide_update_page(app, (int)TUTORIAL_STEPS_COUNT,
                                           manual_screen_start_exercise,
                                           whm_manual_close);

    practice_screen_manual_layout(app, UIModalPracticeManual, (int)TUTORIAL_STEPS_COUNT,
                                  manual_screen_tutorial_top_gap(), footer_content_pad,
                                  0, &layout);
    if(app_draw_close_title_bar(app,
                            GetLocaleText("practice_manual_button"),
                            layout.title_h))
        whm_manual_close(app, 0);

    {
        int responsive_max_w = (int)(view_width * 0.96f);
        int max_content_w = manual_screen_tutorial_max_width();
        WhmManualScrollPageContext page_ctx;
        UIScrollPage page;

        if(responsive_max_w > max_content_w)
            responsive_max_w = max_content_w;
        if(responsive_max_w < ScaleUIPx(280))
            responsive_max_w = ScaleUIPx(280);

        page_ctx = (WhmManualScrollPageContext){app, step};
        page = BeginUIScrollPage((UIScrollPageSpec){
            .y = layout.content_y,
            .height = layout.content_h,
            .max_content_width = responsive_max_w,
            .min_content_width = ScaleUIPx(200),
            .side_padding = manual_screen_tutorial_side_padding(),
            .scroll_offset = &app->manual_scroll,
            .content_height = whm_manual_scroll_page_content_height,
            .user_data = &page_ctx
        });

        int y = page.content_y;
        if(step == 0) {
            int img_h = ScaleUIPx(170);
            DrawUITutorialImage(app->whm.image_1, "practices/whm/1.png",
                                   page.content_x, y, page.content_w, img_h);
            y += img_h + ScaleUIPx(22);

            draw_tutorial_paragraph(app, 0, page.content_x, &y, page.content_w);
        } else if(step == 1) {
            draw_tutorial_paragraph(app, 1, page.content_x, &y, page.content_w);
        } else if(step == 2) {
            int speed = app->inbe.speed_level;
            draw_tutorial_paragraph(app, 2, page.content_x, &y, page.content_w);
            y += ScaleUIPx(20);  /* Increased spacing between text and circle */

            update_preview_bounds(&app->settings_preview, page.content_w, ScaleUIPx(132));
            apply_settings(&app->settings_preview, speed, app->inbe.max_rounds,
                           int_from_count(app->inbe.maxbreaths), app->inbe.pause_seconds);
            app->settings_preview.progressive_speed = 0;
            inbestep(&app->settings_preview);
            if(app->settings_preview.phase != InbePhaseBreathe) {
                reset_settings_preview(app);
                update_preview_bounds(&app->settings_preview, page.content_w, ScaleUIPx(132));
                apply_settings(&app->settings_preview, speed, app->inbe.max_rounds,
                               int_from_count(app->inbe.maxbreaths), app->inbe.pause_seconds);
                app->settings_preview.progressive_speed = 0;
            }
            draw_preview_inbe(&app->settings_preview,
                              page.content_x + page.content_w / 2, y + ScaleUIPx(40));
            y += (int)(app->settings_preview.rmax * 0.72f) + ScaleUIPx(54);

            if(DrawUISlider(10, page.content_x, y, page.content_w, GetLocaleText("speed_label"), SETTINGS_SPEED_MIN,
                           SETTINGS_SPEED_MAX, &speed, "")) {
                apply_settings(&app->inbe, speed, app->inbe.max_rounds,
                               int_from_count(app->inbe.maxbreaths), app->inbe.pause_seconds);
                apply_settings(&app->settings_preview, speed, app->inbe.max_rounds,
                               int_from_count(app->inbe.maxbreaths), app->inbe.pause_seconds);
                app->settings_preview.progressive_speed = 0;
                app->settings_dirty = 1;
            }
        } else if(step == 3) {
            int hold_preview_radius = ScaleUIPx(54);
            int hold_preview_extent = hold_preview_radius + ScaleUIPx(8) + ScaleUIPx(5);
            int breath_button_hover = 0;
            int center_x = page.content_x + page.content_w / 2;
            y += hold_preview_extent;
            draw_tutorial_hold_preview(app, center_x, y, hold_preview_radius);
            y += hold_preview_extent + ScaleUIPx(24);
            DrawUITextButton(center_x, y, GetLocaleText("breath_button"), &breath_button_hover);
            y += ScaleUIPx(42) + ScaleUIPx(18);
            draw_tutorial_paragraph(app, 3, page.content_x, &y, page.content_w);
        } else {
            int img_h = ScaleUIPx(170);
            DrawUITutorialImage(app->whm.image_2, "practices/whm/2.png",
                                   page.content_x, y, page.content_w, img_h);
            y += img_h + ScaleUIPx(22);
            draw_tutorial_paragraph(app, 4, page.content_x, &y, page.content_w);
        }
        EndUIScrollPage(page);
    }

    manual_screen_guide_draw_nav(app, (ManualGuideNav){
        .page = step,
        .page_count = (int)TUTORIAL_STEPS_COUNT,
        .y = layout.nav_y,
        .h = layout.nav_h,
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
