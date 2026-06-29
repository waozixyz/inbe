#include "pet_screen.h"

#include "app.h"
#include "flint_color.h"
#include "flint_scaling.h"
#include "flint_text.h"
#include "flint_theme.h"
#include "flint_ui.h"
#include "raylib.h"

#include <math.h>

extern int view_height;
extern int view_width;

static void
pet_screen_ensure_assets(InbeApp *app)
{
    if(app != NULL && app->pet.egg.id == 0)
        app->pet.egg = app_load_asset_texture("pet/egg1.png");
}

void
pet_screen_draw(InbeApp *app)
{
    const char *text = "Gamification in progress...";
    const char *subtext = "Preview only. This tab is not implemented yet.";
    FlintUIParagraph paragraph;
    int reserved_bottom;
    int content_h;
    int center_x = view_width / 2;
    int center_y;
    int egg_size;
    int text_w;
    int text_x;
    int text_y;
    float t;
    float scale;
    Color muted = flint_darken(flint_theme_get_text(), 34);

    if(app == NULL)
        return;
    reserved_bottom = app_content_bottom_reserved(app);
    content_h = view_height - reserved_bottom;
    if(content_h < flint_px(120))
        content_h = flint_px(120);
    center_y = content_h / 2 - flint_px(18);
    pet_screen_ensure_assets(app);

    egg_size = view_width < view_height ? view_width : view_height;
    egg_size = egg_size * 42 / 100;
    if(egg_size < flint_px(120))
        egg_size = flint_px(120);
    if(egg_size > flint_px(240))
        egg_size = flint_px(240);

    t = (float)app->inbe.frame / 60.0f;
    scale = 1.0f + 0.035f * sinf(t * 2.0f);

    if(app->pet.egg.id != 0) {
        Rectangle src = {0, 0, (float)app->pet.egg.width, (float)app->pet.egg.height};
        Rectangle dst = {(float)center_x, (float)center_y,
                         (float)egg_size * scale, (float)egg_size * scale};
        Vector2 origin = {dst.width / 2.0f, dst.height / 2.0f};
        DrawTexturePro(app->pet.egg, src, dst, origin, 0.0f, WHITE);
    }

    text_w = view_width - flint_px(48);
    if(text_w > flint_px(360))
        text_w = flint_px(360);
    if(text_w < flint_px(160))
        text_w = flint_px(160);
    text_x = center_x - text_w / 2;
    text_y = center_y + egg_size / 2 + flint_px(28);

    paragraph = (FlintUIParagraph){
        .text = text,
        .width = text_w,
        .font = FLINT_TEXT_16,
        .line_gap = flint_px(3),
        .color = flint_theme_get_text()
    };
    flint_ui_paragraph_draw(paragraph, text_x, &text_y);
    text_y += flint_px(6);
    paragraph = (FlintUIParagraph){
        .text = subtext,
        .width = text_w,
        .font = FLINT_TEXT_12,
        .line_gap = flint_px(3),
        .color = muted
    };
    flint_ui_paragraph_draw(paragraph, text_x, &text_y);
}
