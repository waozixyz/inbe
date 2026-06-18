#include "meditation_practice.h"

#include "app.h"
#include "meditation_music.h"
#include "locale.h"
#include "theme.h"
#include "flint_ui.h"
#include "raylib.h"

extern int view_width;
extern int view_height;

static int
meditation_config_content_height(InbeApp *app, int content_w)
{
    return meditation_music_measure_settings(app, content_w, 1, 1);
}

void
meditation_config_screen_draw(InbeApp *app)
{
    int title_h = ui_screen_header_height();
    int content_x;
    int content_w;
    int responsive_max_w = (int)(view_width * 0.96f);
    int max_content_w = flint_px(CONTENT_MAX_W);
    int min_content_w = flint_px(320);
    int scroll_y;
    int scroll_h;
    int content_h;
    int controls_w;
    FlintUIScrollArea scroll_area;
    FlintUIScrollView scroll_view;
    FlintUIHeader header;

    if(responsive_max_w > max_content_w)
        responsive_max_w = max_content_w;
    if(responsive_max_w < min_content_w)
        responsive_max_w = min_content_w;
    flint_centered_column(responsive_max_w, flint_page_side_padding(), &content_x, &content_w);

    header = ui_draw_title_header(title_h, locale_get("exercise_meditation"),
                                  (Texture2D){0}, app->icons[UI_ICON_TYPE_X]);
    if(header.right_clicked) {
        if(app->settings_dirty)
            save_settings(app);
        meditation_practice_leave_config(app);
        app->settings_scroll = 0;
        app->inbe.screen = InbeScreenStart;
    }

    scroll_y = title_h + flint_px(16);
    scroll_h = view_height - scroll_y;
    if(scroll_h < 0)
        scroll_h = 0;

    controls_w = content_w;
    for(int i = 0; i < 3; i++) {
        FlintUIScrollView measured;
        content_h = meditation_config_content_height(app, controls_w);
        scroll_area = (FlintUIScrollArea){
            .bounds = {0.0f, (float)scroll_y, (float)view_width, (float)scroll_h},
            .content_height = content_h,
            .content_x = content_x,
            .content_width = content_w,
            .scroll_offset = &app->settings_scroll,
            .wheel_step = flint_px(42),
            .scrollbar_x = view_width - flint_px(8)
        };
        measured = ui_scroll_container_measure(scroll_area);
        if(measured.content_w == controls_w)
            break;
        controls_w = measured.content_w;
    }

    content_h = meditation_config_content_height(app, controls_w);
    scroll_area = (FlintUIScrollArea){
        .bounds = {0.0f, (float)scroll_y, (float)view_width, (float)scroll_h},
        .content_height = content_h,
        .content_x = content_x,
        .content_width = content_w,
        .scroll_offset = &app->settings_scroll,
        .wheel_step = flint_px(42),
        .scrollbar_x = view_width - flint_px(8)
    };
    scroll_view = ui_scroll_container_begin(scroll_area);
    {
        int y = scroll_view.content_y;
        meditation_music_draw_settings(app, scroll_view.content_x, scroll_view.content_w,
                                       &y, 1, 1);
    }
    ui_scroll_container_end(scroll_area, scroll_view);
    meditation_music_draw_dropdown_menu(app);
}
