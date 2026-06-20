#include "ui.h"

void
ui_draw_info_rows(FlintUIInfoRows rows)
{
    Color background = rows.background.a != 0
                           ? rows.background
                           : flint_darken(c_bg, 6);
    Color separator = rows.separator.a != 0
                          ? rows.separator
                          : flint_darken(c_bg, 30);
    Color default_text = rows.default_text.a != 0 ? rows.default_text : c_text;
    int row_h = rows.row_height > 0 ? rows.row_height : flint_px(32);
    int padding_x = rows.padding_x > 0 ? rows.padding_x : flint_px(10);

    if(rows.rows == NULL || rows.row_count <= 0 || rows.width <= 0 || row_h <= 0)
        return;

    DrawRectangle(rows.x, rows.y, rows.width, row_h * rows.row_count,
                  background);
    for(int i = 0; i < rows.row_count; i++) {
        const FlintUIInfoRow *row = &rows.rows[i];
        int y = rows.y + i * row_h;
        int font = row->font > 0 ? row->font : flint_ui_font();
        Color text = row->color.a != 0 ? row->color : default_text;

        if(i > 0)
            DrawLine(rows.x, y, rows.x + rows.width, y, separator);
        flint_ui_draw_text_left_in_rect(row->text ? row->text : "",
                                        (Rectangle){(float)(rows.x + padding_x),
                                                    (float)y,
                                                    (float)(rows.width - padding_x * 2),
                                                    (float)row_h},
                                        font, text);
    }
}

int
ui_draw_button_row(FlintUIButtonRow row)
{
    int clicked = -1;
    int gap = row.gap > 0 ? row.gap : flint_px(10);
    int button_w;

    if(row.items == NULL || row.count <= 0 || row.width <= 0 || row.height <= 0)
        return -1;

    button_w = (row.width - gap * (row.count - 1)) / row.count;
    if(button_w <= 0)
        return -1;

    for(int i = 0; i < row.count; i++) {
        int hover = 0;
        int x = row.x + i * (button_w + gap);

        if(ui_draw_generic_button(x, row.y, button_w, row.height,
                                  row.items[i].label,
                                  row.items[i].style,
                                  row.items[i].disabled,
                                  &hover))
            clicked = i;
    }

    return clicked;
}
