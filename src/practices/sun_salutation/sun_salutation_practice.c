#include "sun_salutation_practice.h"

#include "app.h"
#include "flint_text.h"
#include "flint_theme.h"
#include "flint_ui.h"

#include <stdio.h>

extern int view_width;

void
sun_salutation_practice_init(InbeApp *app)
{
    char path[64];

    if(app == NULL)
        return;
    if(app->sun_salutation.repetitions < 2 || app->sun_salutation.repetitions > 12)
        app->sun_salutation.repetitions = 3;
    for(int i = 0; i < 8; i++) {
        if(app->sun_salutation.poses[i].id != 0)
            continue;
        snprintf(path, sizeof(path), "practices/sunsalutation/pos_%04d.png", i + 1);
        app->sun_salutation.poses[i] = app_load_asset_texture(path);
    }
}

void
sun_salutation_practice_destroy(InbeApp *app)
{
    if(app == NULL)
        return;
    for(int i = 0; i < 8; i++) {
        app_unload_texture(app->sun_salutation.poses[i]);
        app->sun_salutation.poses[i] = (Texture2D){0};
    }
}

void
sun_salutation_config_screen_draw(InbeApp *app)
{
    int content_x;
    int content_w;
    int y;
    int title_w;
    int value;
    int title_font = FLINT_TEXT_24;

    if(app == NULL)
        return;

    flint_centered_column(CONTENT_MAX_W, CONTENT_SIDE_PAD, &content_x, &content_w);
    y = app_content_top_reserved(app) + flint_px(28);

    title_w = flint_text_measure("Sun Salutation", title_font);
    flint_text_draw("Sun Salutation", (view_width - title_w) / 2, y, title_font,
                    flint_theme_get_text());
    y += flint_px(56);

    value = app->sun_salutation.repetitions;
    if(value < 2 || value > 12)
        value = 3;
    if(ui_draw_slider(610, content_x, y, content_w, "Repetitions", 2, 12, &value, "x")) {
        app->sun_salutation.repetitions = value;
        save_settings(app);
    }
}
