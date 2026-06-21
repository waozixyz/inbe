#include "manual_screen.h"

#include "app.h"
#include "flint_locale.h"
#include "flint_ui.h"
#include "practices/practice_registry.h"
#include "theme.h"
#include <stdio.h>

extern int view_width;

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
    return flint_px(42);
}

void
manual_screen_guide_draw_nav(InbeApp *app, ManualGuideNav nav)
{
    int page;
    int h;
    int button_size;
    int button_pad;
    int max_inner_w;
    int inner_w;
    int inner_x;
    int button_y;
    char page_text[32];
    int font = flint_ui_font();
    int text_w;

    if(app == NULL || nav.page_count <= 1 || nav.start == NULL)
        return;

    page = clampi(nav.page, 0, nav.page_count - 1);
    h = nav.h > 0 ? nav.h : manual_screen_guide_nav_height();
    button_size = view_width < flint_px(360) ? flint_px(32) : flint_px(36);
    button_pad = flint_px(8);
    max_inner_w = flint_px(260);
    inner_w = view_width - flint_px(24);
    if(inner_w > max_inner_w)
        inner_w = max_inner_w;
    if(inner_w < button_size * 2 + flint_px(72))
        inner_w = button_size * 2 + flint_px(72);
    if(inner_w > view_width)
        inner_w = view_width;
    inner_x = (view_width - inner_w) / 2;
    button_y = nav.y + (h - button_size) / 2;

    DrawRectangle(0, nav.y, view_width, h, flint_darken(theme_get_bg(), 5));
    DrawLine(0, nav.y + h - 1, view_width, nav.y + h - 1,
             flint_darken(theme_get_bg(), 28));

    if(flint_ui_icon_button((FlintUIIconButton){
           .bounds = {(float)inner_x, (float)button_y,
                      (float)button_size, (float)button_size},
           .icon = app->icons[UI_ICON_TYPE_BACKWARD],
           .icon_size = flint_px(20),
           .icon_padding = button_pad,
           .disabled = page <= 0 && !nav.show_left_on_first,
           .background = theme_get_button(),
           .hover_background = theme_get_button_hover(),
           .icon_color = theme_get_text(),
           .border = flint_darken(theme_get_button(), 35)
       })) {
        if(page > 0) {
            app->tutorial_step = page - 1;
            app->manual_scroll = 0;
        } else if(nav.show_left_on_first) {
            if(nav.close != NULL)
                nav.close(app, 1);
            else
                nav.start(app);
        }
    }

    snprintf(page_text, sizeof(page_text), "%d/%d", page + 1, nav.page_count);
    text_w = flint_text_measure(page_text, font);
    flint_text_draw(page_text, inner_x + (inner_w - text_w) / 2,
                    flint_text_y(page_text, nav.y, h, font), font,
                    theme_get_text());

    if(flint_ui_icon_button((FlintUIIconButton){
           .bounds = {(float)(inner_x + inner_w - button_size), (float)button_y,
                      (float)button_size, (float)button_size},
           .icon = app->icons[page >= nav.page_count - 1 ? UI_ICON_TYPE_CHECK
                                                          : UI_ICON_TYPE_FORWARD],
           .icon_size = flint_px(20),
           .icon_padding = button_pad,
           .background = theme_get_button(),
           .hover_background = theme_get_button_hover(),
           .icon_color = theme_get_text(),
           .border = flint_darken(theme_get_button(), 35)
       })) {
        if(page == nav.page_count - 1) {
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
