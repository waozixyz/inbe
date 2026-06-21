#include "meditation_practice.h"

#include "app.h"
#include "flint_locale.h"
#include "meditation_music.h"
#include "theme.h"
#include "flint_ui.h"
#include "raylib.h"

extern int view_width;
extern int view_height;

enum {
    MEDITATION_MANUAL_PAGES = 2
};

static void
meditation_manual_start(InbeApp *app)
{
    mark_exercise_manual_seen(app, EXERCISE_MEDITATION);
    app->tutorial_step = 0;
    app->manual_scroll = 0;
    meditation_practice_start(app);
}

void
meditation_manual_close(InbeApp *app, int mark_seen)
{
    if(app == NULL)
        return;
    if(mark_seen)
        mark_exercise_manual_seen(app, EXERCISE_MEDITATION);
    app->tutorial_step = 0;
    app->manual_scroll = 0;
    app_switch_screen(app, InbeScreenStart);
}

static void
draw_intro_page(InbeApp *app, int content_x, int content_w, int *y)
{
    int img_h = flint_px(190);
    FlintUIParagraph paragraph;

    ui_draw_tutorial_image(app->meditation.image_1, "practices/meditation/1.jpg",
                           content_x, *y, content_w, img_h);
    *y += img_h + flint_px(22);

    paragraph = (FlintUIParagraph){
        .text = locale_get("meditation_manual_intro_text"),
        .icon = (Texture2D){0},
        .icon_type = UI_ICON_TYPE_NONE,
        .icon_size = 0,
        .width = content_w,
        .font = flint_ui_font(),
        .line_gap = flint_px(8),
        .color = theme_get_text(),
    };
    flint_ui_paragraph_draw(paragraph, content_x, y);
}

static int
intro_page_height(int content_w)
{
    FlintUIParagraph paragraph = {
        .text = locale_get("meditation_manual_intro_text"),
        .icon = (Texture2D){0},
        .icon_type = UI_ICON_TYPE_NONE,
        .icon_size = 0,
        .width = content_w,
        .font = flint_ui_font(),
        .line_gap = flint_px(8),
        .color = theme_get_text(),
    };

    return flint_px(190) + flint_px(22) + flint_ui_paragraph_height(paragraph) +
           flint_px(20);
}

static int
settings_page_height(InbeApp *app, int content_w)
{
    FlintUIParagraph paragraph = {
        .text = locale_get("meditation_manual_text"),
        .icon = (Texture2D){0},
        .icon_type = UI_ICON_TYPE_NONE,
        .icon_size = 0,
        .width = content_w,
        .font = flint_ui_font(),
        .line_gap = flint_px(8),
        .color = theme_get_text(),
    };

    return meditation_music_measure_settings(app, content_w, 0, 1) +
           flint_px(34) + flint_ui_paragraph_height(paragraph) + flint_px(20);
}

typedef struct MeditationManualScrollPageContext {
    InbeApp *app;
    int page;
} MeditationManualScrollPageContext;

static int
meditation_manual_scroll_page_content_height(int content_w, void *user_data)
{
    MeditationManualScrollPageContext *ctx = user_data;

    return ctx->page == 0 ? intro_page_height(content_w)
                          : settings_page_height(ctx->app, content_w);
}

void
meditation_manual_draw(InbeApp *app)
{
    int title_h = ui_screen_header_height();
    int button_h = flint_px(36);
    int footer_y = view_height - flint_px(42);
    int content_y = title_h + flint_px(16);
    int content_h = footer_y - content_y - flint_px(28);
    int responsive_max_w = (int)(view_width * 0.90f);
    int max_content_w = flint_px(520);
    int page = clampi(app->tutorial_step, 0, MEDITATION_MANUAL_PAGES - 1);
    int content_x = 0;
    int content_w = 0;

    if(responsive_max_w > max_content_w)
        responsive_max_w = max_content_w;
    if(responsive_max_w < flint_px(280))
        responsive_max_w = flint_px(280);

    app->tutorial_step = page;
    if(ui_draw_screen_header(locale_get("meditation_manual_title"), 1))
        meditation_manual_close(app, 0);

    if(content_h < flint_px(120))
        content_h = flint_px(120);

    {
        MeditationManualScrollPageContext page_ctx = {app, page};
        FlintUIScrollPage scroll_page = ui_scroll_page_begin((FlintUIScrollPageSpec){
            .y = content_y,
            .height = content_h,
            .max_content_width = responsive_max_w,
            .min_content_width = flint_px(280),
            .scroll_offset = &app->manual_scroll,
            .content_height = meditation_manual_scroll_page_content_height,
            .user_data = &page_ctx
        });
        int y = scroll_page.content_y;

        content_x = scroll_page.content_x;
        content_w = scroll_page.content_w;
        if(page == 0) {
            draw_intro_page(app, content_x, content_w, &y);
        } else {
            FlintUIParagraph paragraph;
            meditation_music_draw_settings(app, content_x, content_w,
                                           &y, 0, 1);
            y += flint_px(34);
            paragraph = (FlintUIParagraph){
                .text = locale_get("meditation_manual_text"),
                .icon = (Texture2D){0},
                .icon_type = UI_ICON_TYPE_NONE,
                .icon_size = 0,
                .width = content_w,
                .font = flint_ui_font(),
                .line_gap = flint_px(8),
                .color = theme_get_text(),
            };
            flint_ui_paragraph_draw(paragraph, content_x, &y);
        }
        ui_scroll_page_end(scroll_page);
    }
    if(page == 1)
        meditation_music_draw_dropdown_menu(app);

    {
        int gap = flint_px(10);
        int manual_seen = exercise_manual_seen(app, EXERCISE_MEDITATION);
        int show_left = page > 0 || manual_seen;
        const char *left = page == 0 ? locale_get("tutorial_skip_button")
                                     : locale_get("tutorial_back_button");
        const char *right = page == 0 ? locale_get("tutorial_next_button")
                                      : locale_get("tutorial_start_button");
        FlintUIButtonRowItem buttons[2] = {
            {left, UI_BUTTON_STYLE_PRIMARY, 0},
            {right, UI_BUTTON_STYLE_PRIMARY, 0}
        };
        int clicked;

        clicked = ui_draw_button_row((FlintUIButtonRow){
            .x = content_x,
            .y = footer_y,
            .width = content_w,
            .height = button_h,
            .gap = gap,
            .items = show_left ? buttons : &buttons[1],
            .count = show_left ? 2 : 1
        });

        if(clicked == 0 && show_left) {
            if(page == 0)
                meditation_manual_start(app);
            else {
                app->tutorial_step = 0;
                app->manual_scroll = 0;
            }
        } else if(clicked == (show_left ? 1 : 0)) {
            if(page == 0) {
                app->tutorial_step = 1;
                app->manual_scroll = 0;
            } else {
                meditation_manual_start(app);
            }
        }
    }

    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && app->settings_dirty)
        save_settings(app);
}
