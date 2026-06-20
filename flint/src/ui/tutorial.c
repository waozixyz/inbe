#include "ui.h"

void
ui_draw_tutorial_image_placeholder(const char *label, int x, int y, int w, int h)
{
    DrawRectangle(x, y, w, h, flint_darken(c_bg, 12));
    ui_draw_bevel(x, y, w, h, flint_darken(c_bg, 45), flint_lighten(c_bg, 35));
    int font = flint_ui_font();
    int tw = flint_text_measure(label, font);
    flint_text_draw(label, x + w / 2 - tw / 2, flint_ui_text_y(label, y, h, font), font, c_text);
}

void
ui_draw_tutorial_image(Texture2D texture, const char *fallback, int x, int y, int w, int h)
{
    if(texture.id == 0) {
        ui_draw_tutorial_image_placeholder(fallback, x, y, w, h);
        return;
    }

    float scale_x = (float)w / (float)texture.width;
    float scale_y = (float)h / (float)texture.height;
    float scale = scale_x < scale_y ? scale_x : scale_y;
    float dst_w = (float)texture.width * scale;
    float dst_h = (float)texture.height * scale;
    Rectangle src = {0, 0, (float)texture.width, (float)texture.height};
    Rectangle dst = {x + ((float)w - dst_w) * 0.5f, y + ((float)h - dst_h) * 0.5f, dst_w, dst_h};

    DrawTexturePro(texture, src, dst, (Vector2){0}, 0, WHITE);
}

/* ================================================================
 * MODAL DIALOGS
 * ================================================================ */
