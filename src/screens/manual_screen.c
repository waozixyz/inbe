#include "manual_screen.h"

#include "app.h"
#include "locale.h"
#include "ui.h"
#include "practices/practice_registry.h"
#include "theme.h"
#include <stdio.h>

extern int view_width;
extern int view_height;

void
manual_screen_reset_layouts(InbeApp *app)
{
    if(app == NULL)
        return;
    app->manual_scroll = 0;
}

int
manual_screen_guide_update_page(InbeApp *app, int page_count,
                                ManualGuideStartFn start,
                                ManualGuideCloseFn close)
{
    int page;
    int previous_page;

    if(app == NULL || page_count <= 0)
        return 0;

    page = clampi(app->tutorial_step, 0, page_count - 1);
    previous_page = page;

    if(IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_ENTER)) {
        if(page < page_count - 1)
            page++;
        else if(close != NULL)
            close(app, 1);
        else if(start != NULL)
            start(app);
    }
    if(IsKeyPressed(KEY_LEFT) && page > 0)
        page--;
    if(IsKeyPressed(KEY_ESCAPE) && close != NULL)
        close(app, 0);

    page = clampi(page, 0, page_count - 1);
    if(page != previous_page)
        app->manual_scroll = 0;
    app->tutorial_step = page;
    return page;
}

int
manual_screen_guide_nav_height(void)
{
    return 0;
}

void
manual_screen_guide_draw_nav(InbeApp *app, ManualGuideNav nav)
{
    int page;
    int button_size;
    int button_pad;
    int button_y;
    int side_pad;

    if(app == NULL || nav.page_count <= 1 || nav.start == NULL)
        return;

    page = clampi(nav.page, 0, nav.page_count - 1);
    button_size = view_width < ScaleUIPx(360) ? ScaleUIPx(32) : ScaleUIPx(36);
    button_pad = ScaleUIPx(8);
    side_pad = ScaleUIPx(8);
    button_y = (view_height - button_size) / 2;

    if(page > 0 || nav.show_left_on_first) {
        if(DrawUIIconButton((UIIconButton){
               .bounds = {(float)side_pad, (float)button_y,
                          (float)button_size, (float)button_size},
               .icon = app->icons[UI_ICON_TYPE_BACKWARD],
               .icon_size = ScaleUIPx(20),
               .icon_padding = button_pad,
           })) {
            if(page > 0) {
                app->tutorial_step = page - 1;
                app->manual_scroll = 0;
            } else if(nav.close != NULL) {
                app->modal_input_block_frame = app->inbe.frame;
                nav.close(app, 0);
            }
        }
    }

    if(DrawUIIconButton((UIIconButton){
           .bounds = {(float)(view_width - side_pad - button_size), (float)button_y,
                      (float)button_size, (float)button_size},
           .icon = app->icons[page >= nav.page_count - 1 ? UI_ICON_TYPE_CHECK
                                                          : UI_ICON_TYPE_FORWARD],
           .icon_size = ScaleUIPx(20),
           .icon_padding = button_pad,
       })) {
        if(page == nav.page_count - 1) {
            app->modal_input_block_frame = app->inbe.frame;
            if(nav.close != NULL)
                nav.close(app, 1);
            else
                nav.start(app);
        } else {
            app->tutorial_step = page + 1;
            app->manual_scroll = 0;
        }
    }
}

void
manual_screen_close_tutorial(InbeApp *app, int mark_seen)
{
    const PracticeDefinition *practice;

    if(app == NULL)
        return;
    practice = practice_get(app->exercise_type);
    if(practice->close_manual != NULL)
        practice->close_manual(app, mark_seen);
}

void
manual_screen_draw(InbeApp *app)
{
    const PracticeDefinition *practice;

    if(app == NULL)
        return;
    practice = practice_get(app->exercise_type);
    if(practice->draw_manual != NULL)
        practice->draw_manual(app);
}
