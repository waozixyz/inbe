#include "meditation_practice.h"

#include "app.h"
#include "meditation_music.h"
#include "flint_locale.h"
#include "flint_theme.h"
#include "flint_ui.h"
#include "raylib.h"

#include <stdio.h>

extern int view_width;
extern int view_height;

static const int meditation_config_preset_count = 5;

static int
meditation_config_duration_height(int content_w)
{
    int columns = content_w >= flint_px(520) ? 3 : 2;
    int count = meditation_config_preset_count + 1;
    int rows = (count + columns - 1) / columns;

    return flint_px(30) + rows * flint_px(42) + (rows - 1) * flint_px(8) +
           flint_px(96) + flint_px(78) + flint_px(18);
}

static int
meditation_config_content_height(InbeApp *app, int content_w)
{
    int integrated = app->inbe.screen == InbeScreenStart &&
                     app->modal.type != UIModalPracticeConfig;
    return meditation_config_duration_height(content_w) +
           meditation_music_measure_settings(app, content_w, 1, 1) +
           (integrated ? app_content_bottom_reserved(app) : 0) + flint_px(24);
}

static void
meditation_config_save(InbeApp *app)
{
    if(app == NULL)
        return;
    app->settings_dirty = 1;
    save_settings(app);
}

static void
meditation_config_draw_duration(InbeApp *app, int x, int w, int *y)
{
    const char *labels[] = {
        locale_get("duration_5m"),
        locale_get("duration_15m"),
        locale_get("duration_30m"),
        locale_get("duration_1h"),
        locale_get("duration_2h"),
        locale_get("meditation_duration_custom")
    };
    int mode;
    int columns;
    int gap = flint_px(8);
    int btn_h = flint_px(36);
    int btn_w;
    int row_y;
    int custom_row_y;
    int toggle_value;
    int hover = 0;
    char custom_text[64];

    if(app == NULL || y == NULL)
        return;

    mode = clampi(app->meditation.duration_mode, 0, meditation_config_preset_count);
    app->meditation.duration_mode = mode;
    app->meditation.custom_minutes = clampi(app->meditation.custom_minutes, 1, 240);

    flint_text_draw(locale_get("meditation_duration_setting"), x, *y,
                    flint_ui_font(), flint_theme_get_text());
    *y += flint_px(30);

    columns = w >= flint_px(520) ? 3 : 2;
    btn_w = (w - gap * (columns - 1)) / columns;
    for(int i = 0; i < meditation_config_preset_count + 1; i++) {
        int col = i % columns;
        int row = i / columns;
        int bx = x + col * (btn_w + gap);
        int by = *y + row * (btn_h + gap);
        int style = i == mode ? UI_BUTTON_STYLE_PRIMARY : UI_BUTTON_STYLE_SECONDARY;

        if(ui_draw_generic_button(bx, by, btn_w, btn_h, labels[i], style, 0, &hover)) {
            app->meditation.duration_mode = i;
            meditation_config_save(app);
        }
    }

    row_y = *y + ((meditation_config_preset_count + 1 + columns - 1) / columns) *
                  (btn_h + gap);
    *y = row_y + flint_px(8);

    custom_row_y = *y;
    snprintf(custom_text, sizeof(custom_text), "%s: %d min",
             locale_get("meditation_custom_minutes"), app->meditation.custom_minutes);
    flint_text_draw(custom_text, x, custom_row_y,
                    flint_ui_font(), flint_theme_get_text());

    {
        const char *adjust_labels[] = {"-5", "-1", "+1", "+5"};
        const int adjust_values[] = {-5, -1, 1, 5};
        int adjust_w = flint_px(50);
        int adjust_total = adjust_w * 4 + gap * 3;
        int adjust_x = x + (w - adjust_total) / 2;
        int adjust_y = custom_row_y + flint_px(34);

        for(int i = 0; i < 4; i++) {
            if(ui_draw_generic_button(adjust_x + i * (adjust_w + gap), adjust_y,
                                      adjust_w, btn_h, adjust_labels[i],
                                      UI_BUTTON_STYLE_SECONDARY, 0, &hover)) {
                app->meditation.custom_minutes =
                    clampi(app->meditation.custom_minutes + adjust_values[i], 1, 240);
                app->meditation.duration_mode = meditation_config_preset_count;
                meditation_config_save(app);
            }
        }
    }
    *y += flint_px(96);

    flint_text_draw(locale_get("meditation_extend_controls_label"), x, *y,
                    flint_ui_font(), flint_theme_get_text());
    toggle_value = app->meditation.show_extend_controls;
    if(ui_draw_toggle_switch(x, *y + flint_px(26), flint_px(56), flint_px(30),
                             &toggle_value, locale_get("toggle_off"),
                             locale_get("toggle_on"))) {
        app->meditation.show_extend_controls = toggle_value;
        meditation_config_save(app);
    }
    *y += flint_px(78);
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
    int integrated = app->inbe.screen == InbeScreenStart &&
                     app->modal.type != UIModalPracticeConfig;
    int title_h = integrated ? app_content_top_reserved(app) : ui_screen_header_height();
    int scroll_y;
    int scroll_h;
    int bottom_reserved = integrated ? app_content_bottom_reserved(app) : 0;

    if(!integrated) {
        FlintUIHeader header = ui_draw_title_header(title_h, locale_get("practice_config_title"),
                                                    (Texture2D){0}, app->icons[UI_ICON_TYPE_X]);
        if(header.right_clicked) {
            if(app->modal.active && app->modal.type == UIModalPracticeConfig) {
                app_close_modal(app);
            } else {
                if(app->settings_dirty)
                    save_settings(app);
                meditation_practice_leave_config(app);
                app->settings_scroll = 0;
                app->practice_tab = PRACTICE_TAB_PLAY;
                app_switch_screen(app, InbeScreenStart);
            }
        }
    }

    scroll_y = title_h + flint_px(16);
    scroll_h = view_height - scroll_y - bottom_reserved -
               (integrated ? flint_px(8) : 0);
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

        meditation_config_draw_duration(app, page.content_x, page.content_w, &y);
        meditation_music_draw_settings(app, page.content_x, page.content_w,
                                       &y, 1, 1);
        ui_scroll_page_end(page);
    }
    meditation_music_draw_dropdown_menu(app);
}
