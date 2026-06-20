#include "meditation_practice.h"

#include "app.h"
#include "meditation_music.h"
#include "flint_locale.h"
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

typedef struct MeditationConfigScrollPageContext {
    InbeApp *app;
} MeditationConfigScrollPageContext;

static int
meditation_config_scroll_page_content_height(int content_w, void *user_data)
{
    MeditationConfigScrollPageContext *ctx = user_data;

    return meditation_config_content_height(ctx->app, content_w);
}

void
meditation_config_screen_draw(InbeApp *app)
{
    int title_h = ui_screen_header_height();
    int scroll_y;
    int scroll_h;
    FlintUIHeader header;

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

    {
        MeditationConfigScrollPageContext page_ctx = {app};
        FlintUIScrollPage page = ui_scroll_page_begin((FlintUIScrollPageSpec){
            .y = scroll_y,
            .height = scroll_h,
            .max_content_width = flint_px(CONTENT_MAX_W),
            .min_content_width = flint_px(320),
            .scroll_offset = &app->settings_scroll,
            .content_height = meditation_config_scroll_page_content_height,
            .user_data = &page_ctx
        });
        int y = page.content_y;

        meditation_music_draw_settings(app, page.content_x, page.content_w,
                                       &y, 1, 1);
        ui_scroll_page_end(page);
    }
    meditation_music_draw_dropdown_menu(app);
}
