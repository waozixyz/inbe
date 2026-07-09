#include "pet_screen.h"

#include "app.h"
#include "ui_color.h"
#include "locale.h"
#include "ui_scaling.h"
#include "ui_text.h"
#include "theme.h"
#include "ui.h"
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
    const char *text = GetLocaleText("pet_gamification_title");
    const char *subtext = GetLocaleText("pet_gamification_subtitle");
    UIParagraph paragraph;
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
    Color muted = DarkenUIColor(GetThemeText(), 34);

    if(app == NULL)
        return;
    reserved_bottom = app_content_bottom_reserved(app);
    content_h = view_height - reserved_bottom;
    if(content_h < ScaleUIPx(120))
        content_h = ScaleUIPx(120);
    center_y = content_h / 2 - ScaleUIPx(18);
    pet_screen_ensure_assets(app);

    egg_size = view_width < view_height ? view_width : view_height;
    egg_size = egg_size * 42 / 100;
    if(egg_size < ScaleUIPx(120))
        egg_size = ScaleUIPx(120);
    if(egg_size > ScaleUIPx(240))
        egg_size = ScaleUIPx(240);

    t = (float)app->inbe.frame / 60.0f;
    scale = 1.0f + 0.035f * sinf(t * 2.0f);

    if(app->pet.egg.id != 0) {
        Rectangle src = {0, 0, (float)app->pet.egg.width, (float)app->pet.egg.height};
        Rectangle dst = {(float)center_x, (float)center_y,
                         (float)egg_size * scale, (float)egg_size * scale};
        Vector2 origin = {dst.width / 2.0f, dst.height / 2.0f};
        DrawTexturePro(app->pet.egg, src, dst, origin, 0.0f, WHITE);
    }

    text_w = view_width - ScaleUIPx(48);
    if(text_w > ScaleUIPx(360))
        text_w = ScaleUIPx(360);
    if(text_w < ScaleUIPx(160))
        text_w = ScaleUIPx(160);
    text_x = center_x - text_w / 2;
    text_y = center_y + egg_size / 2 + ScaleUIPx(28);

    paragraph = (UIParagraph){
        .text = text,
        .width = text_w,
        .font = UI_TEXT_16,
        .line_gap = ScaleUIPx(3),
        .color = GetThemeText()
    };
    DrawUIParagraph(paragraph, text_x, &text_y);
    text_y += ScaleUIPx(6);
    paragraph = (UIParagraph){
        .text = subtext,
        .width = text_w,
        .font = UI_TEXT_12,
        .line_gap = ScaleUIPx(3),
        .color = muted
    };
    DrawUIParagraph(paragraph, text_x, &text_y);
}
