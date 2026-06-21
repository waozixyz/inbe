#include "meditation_practice.h"

#include "app.h"
#include "flint_locale.h"
#include "screens/manual_screen.h"
#include "flint_theme.h"
#include "flint_ui.h"
#include "raylib.h"

extern int view_width;
extern int view_height;

enum {
    MEDITATION_MANUAL_PAGES = 1
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
    app->practice_tab = PRACTICE_TAB_PLAY;
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
        .width = content_w,
        .line_gap = flint_px(8),
    };
    flint_ui_paragraph_draw(paragraph, content_x, y);
}

static int
intro_page_height(int content_w)
{
    FlintUIParagraph paragraph = {
        .text = locale_get("meditation_manual_intro_text"),
        .width = content_w,
        .line_gap = flint_px(8),
    };

    return flint_px(190) + flint_px(22) + flint_ui_paragraph_height(paragraph) +
           flint_px(20);
}

static int
meditation_manual_scroll_page_content_height(int content_w, void *user_data)
{
    (void)user_data;
    return intro_page_height(content_w);
}

void
meditation_manual_draw(InbeApp *app)
{
    int integrated = app->inbe.screen == InbeScreenStart;
    int title_h = integrated ? app_content_top_reserved(app) : ui_screen_header_height();
    int nav_h = MEDITATION_MANUAL_PAGES > 1 ? manual_screen_guide_nav_height() : 0;
    int nav_y = view_height - app_content_bottom_reserved(app) - nav_h;
    int content_y = title_h + flint_px(16);
    int content_h = nav_y - content_y - flint_px(16);
    int responsive_max_w = (int)(view_width * 0.90f);
    int max_content_w = flint_px(520);
    int page = clampi(app->tutorial_step, 0, MEDITATION_MANUAL_PAGES - 1);

    if(responsive_max_w > max_content_w)
        responsive_max_w = max_content_w;
    if(responsive_max_w < flint_px(280))
        responsive_max_w = flint_px(280);

    page = manual_screen_guide_update_page(app, MEDITATION_MANUAL_PAGES,
                                           meditation_manual_start,
                                           meditation_manual_close);
    if(!integrated && ui_draw_screen_header(locale_get("meditation_manual_title"), 1))
        meditation_manual_close(app, 0);

    if(content_h < flint_px(120))
        content_h = flint_px(120);

    {
        FlintUIScrollPage scroll_page = ui_scroll_page_begin((FlintUIScrollPageSpec){
            .y = content_y,
            .height = content_h,
            .max_content_width = responsive_max_w,
            .min_content_width = flint_px(280),
            .scroll_offset = &app->manual_scroll,
            .content_height = meditation_manual_scroll_page_content_height,
            .user_data = NULL
        });
        int y = scroll_page.content_y;

        draw_intro_page(app, scroll_page.content_x, scroll_page.content_w, &y);
        ui_scroll_page_end(scroll_page);
    }

    manual_screen_guide_draw_nav(app, (ManualGuideNav){
        .page = page,
        .page_count = MEDITATION_MANUAL_PAGES,
        .y = nav_y,
        .h = nav_h,
        .show_left_on_first = exercise_manual_seen(app, EXERCISE_MEDITATION),
        .start = meditation_manual_start,
        .close = meditation_manual_close
    });

    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && app->settings_dirty)
        save_settings(app);
}
