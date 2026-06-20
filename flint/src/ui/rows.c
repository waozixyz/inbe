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
ui_label_text_field_height(FlintUILabelTextField row)
{
    int label_h = row.label_h > 0 ? row.label_h : flint_px(22);
    int field_h = row.field_h > 0 ? row.field_h : flint_px(40);
    int gap = row.gap > 0 ? row.gap : 0;
    int bottom_gap = row.bottom_gap > 0 ? row.bottom_gap : flint_px(24);

    return label_h + gap + field_h + bottom_gap;
}

int
ui_draw_label_text_field(FlintUILabelTextField row, int x, int y, int w)
{
    int label_font = row.label_font > 0 ? row.label_font : flint_ui_font_small();
    int label_h = row.label_h > 0 ? row.label_h : flint_px(22);
    int field_h = row.field_h > 0 ? row.field_h : flint_px(40);
    int gap = row.gap > 0 ? row.gap : 0;
    Color label_color = row.label_color.a != 0 ? row.label_color : flint_darken(c_text, 34);
    FlintUITextField field = row.field;

    flint_text_draw(row.label != NULL ? row.label : "", x, y, label_font, label_color);
    field.bounds = (Rectangle){(float)x, (float)(y + label_h + gap), (float)w, (float)field_h};
    return flint_ui_text_field(field);
}

int
ui_section_label_height(FlintUISectionLabel label)
{
    return label.height > 0 ? label.height : flint_px(24);
}

int
ui_draw_section_label(FlintUISectionLabel label, int x, int y)
{
    int font = label.font > 0 ? label.font : flint_ui_font_small();
    int icon_d = label.icon_diameter > 0 ? label.icon_diameter : flint_px(18);
    Color color = label.color.a != 0 ? label.color : flint_darken(c_text, 34);
    const char *text = label.label != NULL ? label.label : "";
    int label_w;

    flint_text_draw(text, x, y, font, color);
    if(!label.info_button)
        return 0;
    label_w = flint_text_measure(text, font);
    return ui_draw_info_button(x + label_w + flint_px(16),
                               y + font / 2 + flint_px(1), icon_d);
}

int
ui_checkbox_row_height(FlintUICheckboxRow row)
{
    return row.height > 0 ? row.height : flint_px(42);
}

int
ui_draw_checkbox_row(FlintUICheckboxRow row, int x, int y)
{
    if(row.disabled)
        return ui_draw_checkbox_toggle_disabled(x, y, row.label, row.value, 1);
    return ui_draw_checkbox_toggle(x, y, row.label, row.value);
}

int
ui_button_row_height(FlintUIButtonRow row)
{
    return row.height > 0 ? row.height : flint_px(40);
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
