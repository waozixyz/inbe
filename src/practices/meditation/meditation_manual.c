#include "meditation_practice.h"

#include "app.h"
#include "locale.h"
#include "screens/manual_screen.h"
#include "screens/practice_screen.h"
#include "theme.h"
#include "ui.h"
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
    if(app->modal.active && app->modal.type == UIModalPracticeManual) {
        app_close_modal(app);
        return;
    }
    app->tutorial_step = 0;
    app->manual_scroll = 0;
    app->practice_tab = PRACTICE_TAB_PLAY;
    app_switch_screen(app, InbeScreenStart);
}

static void
draw_intro_page(InbeApp *app, int content_x, int content_w, int *y)
{
    int img_h = ScaleUIPx(190);
    UIParagraph paragraph;

    DrawUITutorialImage(app->meditation.image_1, "practices/meditation/1.jpg",
                           content_x, *y, content_w, img_h);
    *y += img_h + ScaleUIPx(22);

    paragraph = (UIParagraph){
        .text = GetLocaleText("meditation_manual_intro_text"),
        .width = content_w,
        .line_gap = ScaleUIPx(8),
    };
    DrawUIParagraph(paragraph, content_x, y);
}

static int
intro_page_height(int content_w)
{
    UIParagraph paragraph = {
        .text = GetLocaleText("meditation_manual_intro_text"),
        .width = content_w,
        .line_gap = ScaleUIPx(8),
    };

    return ScaleUIPx(190) + ScaleUIPx(22) + GetUIParagraphHeight(paragraph) +
           ScaleUIPx(20);
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
    PracticeManualLayout layout;
    int responsive_max_w = (int)(view_width * 0.90f);
    int max_content_w = ScaleUIPx(520);
    int page = clampi(app->tutorial_step, 0, MEDITATION_MANUAL_PAGES - 1);

    if(responsive_max_w > max_content_w)
        responsive_max_w = max_content_w;
    if(responsive_max_w < ScaleUIPx(280))
        responsive_max_w = ScaleUIPx(280);

    page = manual_screen_guide_update_page(app, MEDITATION_MANUAL_PAGES,
                                           meditation_manual_start,
                                           meditation_manual_close);
    practice_screen_manual_layout(app, UIModalPracticeManual, MEDITATION_MANUAL_PAGES,
                                  ScaleUIPx(16), ScaleUIPx(16), ScaleUIPx(120),
                                  &layout);
    if(DrawUIReturnTitleBar(app->icons[UI_ICON_TYPE_RETURN],
                                 GetLocaleText("meditation_manual_title"),
                                 layout.title_h))
        meditation_manual_close(app, 0);

    {
        UIScrollPage scroll_page = BeginUIScrollPage((UIScrollPageSpec){
            .y = layout.content_y,
            .height = layout.content_h,
            .max_content_width = responsive_max_w,
            .min_content_width = ScaleUIPx(280),
            .scroll_offset = &app->manual_scroll,
            .content_height = meditation_manual_scroll_page_content_height,
            .user_data = NULL
        });
        int y = scroll_page.content_y;

        draw_intro_page(app, scroll_page.content_x, scroll_page.content_w, &y);
        EndUIScrollPage(scroll_page);
    }

    manual_screen_guide_draw_nav(app, (ManualGuideNav){
        .page = page,
        .page_count = MEDITATION_MANUAL_PAGES,
        .y = layout.nav_y,
        .h = layout.nav_h,
        .show_left_on_first = exercise_manual_seen(app, EXERCISE_MEDITATION),
        .start = meditation_manual_start,
        .close = meditation_manual_close
    });

    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && app->settings_dirty)
        save_settings(app);
}
