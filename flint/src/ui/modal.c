#include "ui.h"

int
ui_draw_modal(const char *title, const char *message,
               const char *cancel_btn, const char *confirm_btn)
{
    int modal_w = flint_px(280);
    int modal_h = flint_px(160);
    int modal_x = (ui_view_width - modal_w) / 2;
    int modal_y = (ui_view_height - modal_h) / 2;
    int title_font = flint_ui_font();
    int msg_font = flint_ui_font();
    int btn_font = flint_ui_font();
    int btn_h = flint_clamp_px(36, 32, 40);
    int btn_w = flint_px(100);
    int btn_gap = flint_px(12);
    int title_h = flint_px(32);
    int msg_y = modal_y + title_h;
    int btn_y = modal_y + modal_h - btn_h - flint_px(16);

    Vector2 mouse_world = ui_mouse_world();
    int mx = (int)mouse_world.x;
    int my = (int)mouse_world.y;

    /* Dim background */
    DrawRectangle(0, 0, ui_view_width, ui_view_height, (Color){0, 0, 0, 180});

    /* Modal background */
    DrawRectangle(modal_x, modal_y, modal_w, modal_h, c_surface);
    ui_draw_bevel(modal_x, modal_y, modal_w, modal_h, flint_lighten(c_surface, 40), flint_darken(c_surface, 40));

    /* Title */
    int title_w = flint_text_measure(title, title_font);
    flint_text_draw(title, modal_x + (modal_w - title_w) / 2, modal_y + flint_px(12), title_font, c_text);

    /* Message (text layout with icon support) */
    int msg_x = modal_x + flint_px(16);
    int msg_w = modal_w - flint_px(32);

    /* Parse message with icon support - use GEAR icon for warnings */
    FlintTextLayout msg_layout = flint_text_layout_parse(message, g_ui_gear_icon, UI_ICON_TYPE_GEAR, msg_font);
    flint_text_layout_reflow(&msg_layout, msg_w, msg_font, flint_px(4));

    /* Draw the layout */
    flint_text_layout_draw(&msg_layout, msg_x, &msg_y, msg_font, c_text);
    flint_text_layout_free(&msg_layout);

    /* Buttons */
    int cancel_x = modal_x + (modal_w - btn_w * 2 - btn_gap) / 2;
    int confirm_x = cancel_x + btn_w + btn_gap;
    int result = 0;

    /* Cancel button */
    if(mx >= cancel_x && mx < cancel_x + btn_w && my >= btn_y && my < btn_y + btn_h) {
        DrawRectangle(cancel_x, btn_y, btn_w, btn_h, c_button_hover);
        ui_draw_bevel(cancel_x, btn_y, btn_w, btn_h, flint_darken(c_button_hover, 40), flint_lighten(c_button_hover, 40));
        ui_mark_clickable();
        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
            result = 1;
    } else {
        DrawRectangle(cancel_x, btn_y, btn_w, btn_h, c_button);
        ui_draw_bevel(cancel_x, btn_y, btn_w, btn_h, flint_lighten(c_button, 40), flint_darken(c_button, 40));
    }
    int cancel_text_w = flint_text_measure(cancel_btn, btn_font);
    flint_text_draw(cancel_btn, cancel_x + (btn_w - cancel_text_w) / 2, flint_ui_text_y(cancel_btn, btn_y, btn_h, btn_font), btn_font, c_text);

    /* Confirm button */
    if(mx >= confirm_x && mx < confirm_x + btn_w && my >= btn_y && my < btn_y + btn_h) {
        DrawRectangle(confirm_x, btn_y, btn_w, btn_h, c_button_hover);
        ui_draw_bevel(confirm_x, btn_y, btn_w, btn_h, flint_darken(c_button_hover, 40), flint_lighten(c_button_hover, 40));
        ui_mark_clickable();
        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
            result = 2;
    } else {
        DrawRectangle(confirm_x, btn_y, btn_w, btn_h, c_button);
        ui_draw_bevel(confirm_x, btn_y, btn_w, btn_h, flint_lighten(c_button, 40), flint_darken(c_button, 40));
    }
    int confirm_text_w = flint_text_measure(confirm_btn, btn_font);
    flint_text_draw(confirm_btn, confirm_x + (btn_w - confirm_text_w) / 2, flint_ui_text_y(confirm_btn, btn_y, btn_h, btn_font), btn_font, c_text);

    return result;
}

int
ui_draw_modal_3btn(const char *title, const char *message,
                    const char *left_btn, const char *middle_btn, const char *right_btn)
{
    int modal_w = flint_px(300);
    int modal_h = flint_px(160);
    int modal_x = (ui_view_width - modal_w) / 2;
    int modal_y = (ui_view_height - modal_h) / 2;
    int title_font = flint_ui_font();
    int msg_font = flint_ui_font();
    int btn_font = flint_ui_font();
    int btn_h = flint_clamp_px(36, 32, 40);
    int btn_w = flint_px(90);
    int btn_gap = flint_px(8);
    int title_h = flint_px(32);
    int msg_y = modal_y + title_h;
    int btn_y = modal_y + modal_h - btn_h - flint_px(16);

    Vector2 mouse_world = ui_mouse_world();
    int mx = (int)mouse_world.x;
    int my = (int)mouse_world.y;

    /* Dim background */
    DrawRectangle(0, 0, ui_view_width, ui_view_height, (Color){0, 0, 0, 180});

    /* Modal background */
    DrawRectangle(modal_x, modal_y, modal_w, modal_h, c_surface);
    ui_draw_bevel(modal_x, modal_y, modal_w, modal_h, flint_lighten(c_surface, 40), flint_darken(c_surface, 40));

    /* Title */
    int title_w = flint_text_measure(title, title_font);
    flint_text_draw(title, modal_x + (modal_w - title_w) / 2, modal_y + flint_px(12), title_font, c_text);

    /* Message (text layout with icon support) */
    int msg_x = modal_x + flint_px(16);
    int msg_w = modal_w - flint_px(32);

    /* Parse message with icon support - use GEAR icon for warnings */
    FlintTextLayout msg_layout = flint_text_layout_parse(message, g_ui_gear_icon, UI_ICON_TYPE_GEAR, msg_font);
    flint_text_layout_reflow(&msg_layout, msg_w, msg_font, flint_px(4));

    /* Draw the layout */
    flint_text_layout_draw(&msg_layout, msg_x, &msg_y, msg_font, c_text);
    flint_text_layout_free(&msg_layout);

    /* Calculate button positions */
    int total_btn_w = btn_w * 3 + btn_gap * 2;
    int left_x = modal_x + (modal_w - total_btn_w) / 2;
    int middle_x = left_x + btn_w + btn_gap;
    int right_x = middle_x + btn_w + btn_gap;

    int result = 0;

    /* Left button (Cancel) */
    if(mx >= left_x && mx < left_x + btn_w && my >= btn_y && my < btn_y + btn_h) {
        DrawRectangle(left_x, btn_y, btn_w, btn_h, c_button_hover);
        ui_draw_bevel(left_x, btn_y, btn_w, btn_h, flint_darken(c_button_hover, 40), flint_lighten(c_button_hover, 40));
        ui_mark_clickable();
        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
            result = 1;
    } else {
        DrawRectangle(left_x, btn_y, btn_w, btn_h, c_button);
        ui_draw_bevel(left_x, btn_y, btn_w, btn_h, flint_lighten(c_button, 40), flint_darken(c_button, 40));
    }
    int left_text_w = flint_text_measure(left_btn, btn_font);
    flint_text_draw(left_btn, left_x + (btn_w - left_text_w) / 2, flint_ui_text_y(left_btn, btn_y, btn_h, btn_font), btn_font, c_text);

    /* Middle button (Save) - primary action */
    if(mx >= middle_x && mx < middle_x + btn_w && my >= btn_y && my < btn_y + btn_h) {
        DrawRectangle(middle_x, btn_y, btn_w, btn_h, c_button_hover);
        ui_draw_bevel(middle_x, btn_y, btn_w, btn_h, flint_darken(c_button_hover, 40), flint_lighten(c_button_hover, 40));
        ui_mark_clickable();
        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
            result = 2;
    } else {
        DrawRectangle(middle_x, btn_y, btn_w, btn_h, c_button);
        ui_draw_bevel(middle_x, btn_y, btn_w, btn_h, flint_lighten(c_button, 40), flint_darken(c_button, 40));
    }
    int middle_text_w = flint_text_measure(middle_btn, btn_font);
    flint_text_draw(middle_btn, middle_x + (btn_w - middle_text_w) / 2, flint_ui_text_y(middle_btn, btn_y, btn_h, btn_font), btn_font, c_text);

    /* Right button (Discard) */
    if(mx >= right_x && mx < right_x + btn_w && my >= btn_y && my < btn_y + btn_h) {
        DrawRectangle(right_x, btn_y, btn_w, btn_h, c_button_hover);
        ui_draw_bevel(right_x, btn_y, btn_w, btn_h, flint_darken(c_button_hover, 40), flint_lighten(c_button_hover, 40));
        ui_mark_clickable();
        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
            result = 3;
    } else {
        DrawRectangle(right_x, btn_y, btn_w, btn_h, c_button);
        ui_draw_bevel(right_x, btn_y, btn_w, btn_h, flint_lighten(c_button, 40), flint_darken(c_button, 40));
    }
    int right_text_w = flint_text_measure(right_btn, btn_font);
    flint_text_draw(right_btn, right_x + (btn_w - right_text_w) / 2, flint_ui_text_y(right_btn, btn_y, btn_h, btn_font), btn_font, c_text);

    return result;
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
