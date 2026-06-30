#include "settings_ui.h"

#include "flint_locale.h"
#include "flint_ui.h"

static FlintUIParagraph
settings_ui_label_paragraph(const char *label, int w)
{
    return (FlintUIParagraph){
        .text = label,
        .width = w
    };
}

int
settings_ui_toggle_row_height(const char *label, int w)
{
    int label_h;
    int row_h;

    if(w <= 0)
        w = flint_px(160);

    label_h = flint_ui_paragraph_height(settings_ui_label_paragraph(label, w));
    row_h = label_h + flint_px(8) + flint_px(30) + flint_px(22);
    if(row_h < flint_px(76))
        row_h = flint_px(76);
    return row_h;
}

int
settings_ui_draw_toggle_row(int x, int w, int *y, const char *label, int *value)
{
    int label_y;
    int label_h;
    int row_h;
    int toggle_w = flint_px(56);
    int toggle_h = flint_px(30);

    if(y == NULL)
        return 0;

    row_h = settings_ui_toggle_row_height(label, w);
    label_y = *y;
    flint_ui_paragraph_draw(settings_ui_label_paragraph(label, w), x, &label_y);
    label_h = label_y - *y;

    if(ui_draw_toggle_switch(x, *y + label_h + flint_px(8), toggle_w, toggle_h,
                             value, locale_get("toggle_off"),
                             locale_get("toggle_on"))) {
        *y += row_h;
        return 1;
    }

    *y += row_h;
    return 0;
}
