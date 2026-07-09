#include "meditation_practice.h"

#include "app.h"
#include "locale.h"
#include "theme.h"
#include "ui.h"
#include "raylib.h"
#include "screens/practice_screen.h"

#include <stdio.h>

extern int view_width;
extern int view_height;

static const int meditation_config_preset_count = 5;

static int
meditation_config_duration_height(int content_w)
{
    int columns = content_w >= ScaleUIPx(520) ? 3 : 2;
    int count = meditation_config_preset_count + 1;
    int rows = (count + columns - 1) / columns;

    return ScaleUIPx(30) + rows * ScaleUIPx(42) + (rows - 1) * ScaleUIPx(8) +
           ScaleUIPx(96) + ScaleUIPx(78) + ScaleUIPx(18);
}

static int
meditation_config_content_height(InbeApp *app, int content_w)
{
    return meditation_config_duration_height(content_w) +
           (practice_screen_subscreen_integrated(app, UIModalPracticeConfig)
                ? app_content_bottom_reserved(app)
                : 0) +
           ScaleUIPx(24);
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
        GetLocaleText("duration_5m"),
        GetLocaleText("duration_15m"),
        GetLocaleText("duration_30m"),
        GetLocaleText("duration_1h"),
        GetLocaleText("duration_2h"),
        GetLocaleText("meditation_duration_custom")
    };
    int mode;
    int columns;
    int gap = ScaleUIPx(8);
    int btn_h = ScaleUIPx(36);
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

    DrawUIText(GetLocaleText("meditation_duration_setting"), x, *y,
                    GetUIFontSize(), GetThemeText());
    *y += ScaleUIPx(30);

    columns = w >= ScaleUIPx(520) ? 3 : 2;
    btn_w = (w - gap * (columns - 1)) / columns;
    for(int i = 0; i < meditation_config_preset_count + 1; i++) {
        int col = i % columns;
        int row = i / columns;
        int bx = x + col * (btn_w + gap);
        int by = *y + row * (btn_h + gap);
        int style = i == mode ? UI_BUTTON_STYLE_PRIMARY : UI_BUTTON_STYLE_SECONDARY;

        if(DrawUIGenericButton(bx, by, btn_w, btn_h, labels[i], style, 0, &hover)) {
            app->meditation.duration_mode = i;
            meditation_config_save(app);
        }
    }

    row_y = *y + ((meditation_config_preset_count + 1 + columns - 1) / columns) *
                  (btn_h + gap);
    *y = row_y + ScaleUIPx(8);

    custom_row_y = *y;
    snprintf(custom_text, sizeof(custom_text), "%s: %d min",
             GetLocaleText("meditation_custom_minutes"), app->meditation.custom_minutes);
    DrawUIText(custom_text, x, custom_row_y,
                    GetUIFontSize(), GetThemeText());

    {
        const char *adjust_labels[] = {"-5", "-1", "+1", "+5"};
        const int adjust_values[] = {-5, -1, 1, 5};
        int adjust_w = ScaleUIPx(50);
        int adjust_total = adjust_w * 4 + gap * 3;
        int adjust_x = x + (w - adjust_total) / 2;
        int adjust_y = custom_row_y + ScaleUIPx(34);

        for(int i = 0; i < 4; i++) {
            if(DrawUIGenericButton(adjust_x + i * (adjust_w + gap), adjust_y,
                                      adjust_w, btn_h, adjust_labels[i],
                                      UI_BUTTON_STYLE_SECONDARY, 0, &hover)) {
                app->meditation.custom_minutes =
                    clampi(app->meditation.custom_minutes + adjust_values[i], 1, 240);
                app->meditation.duration_mode = meditation_config_preset_count;
                meditation_config_save(app);
            }
        }
    }
    *y += ScaleUIPx(96);

    DrawUIText(GetLocaleText("meditation_extend_controls_label"), x, *y,
                    GetUIFontSize(), GetThemeText());
    toggle_value = app->meditation.show_extend_controls;
    if(DrawUIToggleSwitch(x, *y + ScaleUIPx(26), ScaleUIPx(56), ScaleUIPx(30),
                             &toggle_value, GetLocaleText("toggle_off"),
                             GetLocaleText("toggle_on"))) {
        app->meditation.show_extend_controls = toggle_value;
        meditation_config_save(app);
    }
    *y += ScaleUIPx(78);
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
    PracticeSubscreenLayout layout;

    practice_screen_config_layout(app, UIModalPracticeConfig, ScaleUIPx(16), &layout);
    practice_screen_handle_config_title(app, GetLocaleText("practice_config_title"),
                                        UIModalPracticeConfig,
                                        meditation_practice_leave_config);

    {
        MeditationConfigScrollPageContext page_ctx = {app};
        UIScrollPage page = BeginUIScrollPage((UIScrollPageSpec){
            .y = layout.scroll_y,
            .height = layout.scroll_h,
            .max_content_width = ScaleUIPx(CONTENT_MAX_W),
            .min_content_width = ScaleUIPx(320),
            .scroll_offset = &app->settings_scroll,
            .content_height = meditation_config_scroll_page_content_height,
            .user_data = &page_ctx
        });
        int y = page.content_y;

        meditation_config_draw_duration(app, page.content_x, page.content_w, &y);
        EndUIScrollPage(page);
    }
}
