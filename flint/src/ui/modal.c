#include "ui.h"

static int
ui_modal_button(int x, int y, int w, int h, const char *label, int font,
                Vector2 mouse_world)
{
    Rectangle bounds = {(float)x, (float)y, (float)w, (float)h};
    int active = CheckCollisionPointRec(mouse_world, bounds);
    int hovered = active && ui_hover_effects_enabled();
    Color fill = hovered ? c_button_hover : c_button;
    int text_w;

    DrawRectangle(x, y, w, h, fill);
    ui_draw_bevel(x, y, w, h, flint_lighten(fill, 40), flint_darken(fill, 40));
    if(active)
        ui_mark_clickable();

    text_w = flint_text_measure(label, font);
    flint_text_draw(label, x + (w - text_w) / 2, flint_ui_text_y(label, y, h, font),
                    font, c_text);

    return active && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

int
ui_draw_modal(const char *title, const char *message,
               const char *cancel_btn, const char *confirm_btn)
{
    int modal_w = flint_px(280);
    int modal_x;
    int modal_y;
    int title_font = flint_ui_font();
    int msg_font = flint_ui_font();
    int btn_font = flint_ui_font();
    int btn_h = flint_clamp_px(36, 32, 40);
    int btn_w = flint_px(100);
    int btn_gap = flint_px(12);
    int title_h = flint_px(32);
    int msg_x;
    int msg_y;
    int msg_w = modal_w - flint_px(32);
    int msg_gap = flint_px(18);
    int modal_h;
    int btn_y;
    Vector2 mouse_world = ui_mouse_world();
    FlintTextLayout msg_layout = flint_text_layout_parse(message, g_ui_gear_icon,
                                                          UI_ICON_TYPE_GEAR, msg_font);
    flint_text_layout_reflow(&msg_layout, msg_w, msg_font, flint_px(4));

    modal_h = title_h + flint_text_layout_get_height(&msg_layout) + msg_gap + btn_h + flint_px(20);
    if(modal_h < flint_px(160))
        modal_h = flint_px(160);
    if(modal_h > ui_view_height - flint_px(24))
        modal_h = ui_view_height - flint_px(24);
    modal_x = (ui_view_width - modal_w) / 2;
    modal_y = (ui_view_height - modal_h) / 2;
    msg_x = modal_x + flint_px(16);
    msg_y = modal_y + title_h;
    btn_y = modal_y + modal_h - btn_h - flint_px(16);

    /* Dim background */
    DrawRectangle(0, 0, ui_view_width, ui_view_height, (Color){0, 0, 0, 180});

    /* Modal background */
    DrawRectangle(modal_x, modal_y, modal_w, modal_h, c_surface);
    ui_draw_bevel(modal_x, modal_y, modal_w, modal_h, flint_lighten(c_surface, 40), flint_darken(c_surface, 40));

    /* Title */
    int title_w = flint_text_measure(title, title_font);
    flint_text_draw(title, modal_x + (modal_w - title_w) / 2, modal_y + flint_px(12), title_font, c_text);

    /* Draw the layout */
    flint_text_layout_draw(&msg_layout, msg_x, &msg_y, msg_font, c_text);
    flint_text_layout_free(&msg_layout);

    /* Buttons */
    int cancel_x = modal_x + (modal_w - btn_w * 2 - btn_gap) / 2;
    int confirm_x = cancel_x + btn_w + btn_gap;
    int result = 0;

    if(ui_modal_button(cancel_x, btn_y, btn_w, btn_h, cancel_btn, btn_font, mouse_world))
        result = 1;
    if(ui_modal_button(confirm_x, btn_y, btn_w, btn_h, confirm_btn, btn_font, mouse_world))
        result = 2;

    return result;
}

int
ui_draw_modal_3btn(const char *title, const char *message,
                    const char *left_btn, const char *middle_btn, const char *right_btn)
{
    int modal_w = flint_px(300);
    int modal_x;
    int modal_y;
    int title_font = flint_ui_font();
    int msg_font = flint_ui_font();
    int btn_font = flint_ui_font();
    int btn_h = flint_clamp_px(36, 32, 40);
    int btn_w = flint_px(90);
    int btn_gap = flint_px(8);
    int title_h = flint_px(32);
    int msg_x;
    int msg_y;
    int msg_w = modal_w - flint_px(32);
    int msg_gap = flint_px(18);
    int modal_h;
    int btn_y;
    Vector2 mouse_world = ui_mouse_world();
    FlintTextLayout msg_layout = flint_text_layout_parse(message, g_ui_gear_icon,
                                                          UI_ICON_TYPE_GEAR, msg_font);
    flint_text_layout_reflow(&msg_layout, msg_w, msg_font, flint_px(4));

    modal_h = title_h + flint_text_layout_get_height(&msg_layout) + msg_gap + btn_h + flint_px(20);
    if(modal_h < flint_px(160))
        modal_h = flint_px(160);
    if(modal_h > ui_view_height - flint_px(24))
        modal_h = ui_view_height - flint_px(24);
    modal_x = (ui_view_width - modal_w) / 2;
    modal_y = (ui_view_height - modal_h) / 2;
    msg_x = modal_x + flint_px(16);
    msg_y = modal_y + title_h;
    btn_y = modal_y + modal_h - btn_h - flint_px(16);

    /* Dim background */
    DrawRectangle(0, 0, ui_view_width, ui_view_height, (Color){0, 0, 0, 180});

    /* Modal background */
    DrawRectangle(modal_x, modal_y, modal_w, modal_h, c_surface);
    ui_draw_bevel(modal_x, modal_y, modal_w, modal_h, flint_lighten(c_surface, 40), flint_darken(c_surface, 40));

    /* Title */
    int title_w = flint_text_measure(title, title_font);
    flint_text_draw(title, modal_x + (modal_w - title_w) / 2, modal_y + flint_px(12), title_font, c_text);

    /* Draw the layout */
    flint_text_layout_draw(&msg_layout, msg_x, &msg_y, msg_font, c_text);
    flint_text_layout_free(&msg_layout);

    /* Calculate button positions */
    int total_btn_w = btn_w * 3 + btn_gap * 2;
    int left_x = modal_x + (modal_w - total_btn_w) / 2;
    int middle_x = left_x + btn_w + btn_gap;
    int right_x = middle_x + btn_w + btn_gap;

    int result = 0;

    if(ui_modal_button(left_x, btn_y, btn_w, btn_h, left_btn, btn_font, mouse_world))
        result = 1;
    if(ui_modal_button(middle_x, btn_y, btn_w, btn_h, middle_btn, btn_font, mouse_world))
        result = 2;
    if(ui_modal_button(right_x, btn_y, btn_w, btn_h, right_btn, btn_font, mouse_world))
        result = 3;

    return result;
}

int
ui_paragraph_modal_height(FlintUIParagraphModalMeasure measure)
{
    int width = measure.width > 0 ? measure.width : flint_px(320);
    int header_h = measure.header_h > 0 ? measure.header_h : flint_px(58);
    int button_h = measure.button_h > 0 ? measure.button_h : flint_px(36);
    int line_gap = measure.line_gap > 0 ? measure.line_gap : flint_px(4);
    int font = measure.font > 0 ? measure.font : flint_ui_font();
    int extra_lines = measure.extra_lines > 0 ? measure.extra_lines : 0;
    int min_h = measure.min_height > 0 ? measure.min_height : 0;
    int content_w;
    FlintUIParagraph paragraph;
    int height;

    if(width > ui_view_width - flint_px(24))
        width = ui_view_width - flint_px(24);
    if(width < flint_px(160))
        width = flint_px(160);
    content_w = width - flint_px(36);
    if(content_w < flint_px(120))
        content_w = flint_px(120);
    paragraph = (FlintUIParagraph){
        .text = measure.message,
        .width = content_w,
        .font = font,
        .line_gap = line_gap
    };
    height = header_h +
             flint_ui_paragraph_height(paragraph) +
             extra_lines * (font + line_gap) +
             button_h +
             flint_px(18);
    if(height < min_h)
        height = min_h;
    return height;
}

/* ================================================================
 * SCREEN HEADER (TITLE BAR)
 * ================================================================ */

int
ui_screen_header_height(void)
{
    return flint_clamp_px(60, 48, 60);
}

FlintUIHeader
ui_draw_title_header(int height, const char *title,
                     Texture2D left_icon,
                     Texture2D right_icon)
{
    FlintUIHeader header = {height, 0, 0};
    int icon_size = flint_px(20);
    int icon_padding = flint_px(8);
    int icon_w = icon_size + icon_padding * 2;
    int title_font = flint_ui_title_font(title, ui_view_width - icon_w * 2 - flint_px(48));
    int title_w = flint_text_measure(title, title_font);
    int hover = 0;

    DrawRectangle(0, 0, ui_view_width, height, c_bg);
    DrawLine(0, height - 1, ui_view_width, height - 1, flint_darken(c_button, 18));

    if(left_icon.id != 0) {
        header.left_clicked = ui_draw_icon_btn_padded(flint_px(12), flint_px(12),
                                                      icon_size, icon_padding,
                                                      left_icon, &hover);
    }
    if(right_icon.id != 0) {
        header.right_clicked = ui_draw_icon_btn_padded(ui_view_width - icon_w - flint_px(12),
                                                       flint_px(12), icon_size, icon_padding,
                                                       right_icon, &hover);
    }

    flint_text_draw(title, (ui_view_width - title_w) / 2,
                    flint_ui_text_y(title, 0, height, title_font),
                    title_font, c_text);
    return header;
}

FlintUIPanelFrame
ui_draw_modal_frame(int width, int height, const char *title,
                    Texture2D left_icon,
                    Texture2D right_icon)
{
    FlintUIPanelFrame frame = {0};
    int title_font;
    int icon_size = flint_px(20);
    int icon_padding = flint_px(8);
    int icon_w = icon_size + icon_padding * 2;
    int title_w;
    int hover = 0;

    if(width > ui_view_width - flint_px(24))
        width = ui_view_width - flint_px(24);
    if(height > ui_view_height - flint_px(24))
        height = ui_view_height - flint_px(24);

    frame.w = width;
    frame.h = height;
    frame.x = (ui_view_width - width) / 2;
    frame.y = (ui_view_height - height) / 2;
    frame.content_x = frame.x + flint_px(18);
    frame.content_y = frame.y + flint_px(58);
    frame.content_w = frame.w - flint_px(36);
    frame.content_h = frame.h - flint_px(74);
    title_font = flint_ui_title_font(title, frame.w - icon_w * 2 - flint_px(24));
    title_w = flint_text_measure(title, title_font);

    DrawRectangle(0, 0, ui_view_width, ui_view_height, (Color){0, 0, 0, 180});
    DrawRectangle(frame.x, frame.y, frame.w, frame.h, c_surface);
    ui_draw_bevel(frame.x, frame.y, frame.w, frame.h,
                  flint_lighten(c_surface, 40), flint_darken(c_surface, 40));

    flint_text_draw(title, frame.x + (frame.w - title_w) / 2,
                    frame.y + flint_px(14), title_font, c_text);

    if(left_icon.id != 0) {
        frame.left_clicked = ui_draw_icon_btn_padded(frame.x + flint_px(6),
                                                     frame.y + flint_px(6),
                                                     icon_size, icon_padding,
                                                     left_icon, &hover);
    }
    if(right_icon.id != 0) {
        frame.right_clicked = ui_draw_icon_btn_padded(frame.x + frame.w - icon_w - flint_px(6),
                                                      frame.y + flint_px(6),
                                                      icon_size, icon_padding,
                                                      right_icon, &hover);
    }

    return frame;
}
int
ui_draw_screen_header(const char *title, int show_close)
{
    (void)c_bg; /* unused */
    int title_h = ui_screen_header_height();
    int title_font;
    int close_hover = 0;
    int close_clicked = 0;

    /* Draw header background */
    DrawRectangle(0, 0, ui_view_width, title_h, flint_darken(c_bg, 14));
    DrawLine(0, title_h - 1, ui_view_width, title_h - 1, flint_darken(c_bg, 42));

    /* Draw close button if requested */
    int close_x = ui_view_width - flint_px(40) - ui_icon_btn_padding(UI_ICON_SIZE_TINY);
    int title_x = flint_px(16);
    int title_max_w = show_close ? close_x - title_x - flint_px(12) : ui_view_width - title_x * 2;
    title_font = flint_ui_title_font(title, title_max_w);
    flint_text_draw(title, title_x, flint_ui_text_y(title, 0, title_h, title_font), title_font, c_text);

    if(show_close) {
        close_clicked = ui_draw_icon_btn(close_x, flint_px(8), UI_ICON_SIZE_TINY,
                                         g_ui_x_icon, &close_hover);
    }

    return close_clicked;
}

/* ================================================================
 * SCROLLBAR
 * ================================================================ */
