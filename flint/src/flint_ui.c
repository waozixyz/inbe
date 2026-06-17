#include "flint_ui.h"
#include "flint_dpi.h"
#include "flint.h"
#include "flint_text_layout.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#if defined(PLATFORM_WEB)
#include <emscripten.h>
#endif

/* Global UI state */
int ui_view_width = 320;
int ui_view_height = 560;
static Color c_text, c_bg, c_surface, c_circle, c_button, c_button_hover, c_icon;
static Camera2D g_ui_camera;
static int *g_ui_cursor_clickable = NULL;
static int *g_ui_cursor_disabled = NULL;
static Texture2D g_ui_gear_icon = {0};
static Texture2D g_ui_x_icon = {0};
static int g_ui_slider_active_id = 0;
static int g_ui_input_blocked = 0;

#define UI_FOCUS_MAX_ITEMS 256
static int g_ui_focus_active_id = 0;
static int g_ui_focus_ids[UI_FOCUS_MAX_ITEMS];
static int g_ui_focus_count = 0;
static int g_ui_focus_tab_dir = 0;
static int g_ui_focus_text_input_active = 0;

#define UI_SCISSOR_STACK_MAX 16
static Rectangle g_ui_scissor_stack[UI_SCISSOR_STACK_MAX];
static int g_ui_scissor_stack_count = 0;

static Rectangle
ui_scissor_intersection(Rectangle a, Rectangle b)
{
    float x1 = a.x > b.x ? a.x : b.x;
    float y1 = a.y > b.y ? a.y : b.y;
    float x2 = a.x + a.width < b.x + b.width ? a.x + a.width : b.x + b.width;
    float y2 = a.y + a.height < b.y + b.height ? a.y + a.height : b.y + b.height;

    if(x2 < x1)
        x2 = x1;
    if(y2 < y1)
        y2 = y1;

    return (Rectangle){x1, y1, x2 - x1, y2 - y1};
}

void
ui_begin_scissor(int x, int y, int w, int h)
{
    Rectangle bounds = {x, y, w, h};

    if(w < 0)
        bounds.width = 0;
    if(h < 0)
        bounds.height = 0;
    if(g_ui_scissor_stack_count > 0)
        bounds = ui_scissor_intersection(g_ui_scissor_stack[g_ui_scissor_stack_count - 1], bounds);

    if(g_ui_scissor_stack_count < UI_SCISSOR_STACK_MAX)
        g_ui_scissor_stack[g_ui_scissor_stack_count++] = bounds;

    BeginScissorMode((int)bounds.x, (int)bounds.y,
                     (int)bounds.width, (int)bounds.height);
}

void
ui_end_scissor(void)
{
    EndScissorMode();
    if(g_ui_scissor_stack_count > 0)
        g_ui_scissor_stack_count--;
    if(g_ui_scissor_stack_count > 0) {
        Rectangle bounds = g_ui_scissor_stack[g_ui_scissor_stack_count - 1];
        BeginScissorMode((int)bounds.x, (int)bounds.y,
                         (int)bounds.width, (int)bounds.height);
    }
}

static Vector2
ui_mouse_world(void)
{
    return GetScreenToWorld2D(GetMousePosition(), g_ui_camera);
}

static void
ui_mark_clickable(void)
{
    if(g_ui_cursor_clickable != NULL)
        *g_ui_cursor_clickable = 1;
}

static void
ui_mark_disabled(void)
{
    if(g_ui_cursor_disabled != NULL)
        *g_ui_cursor_disabled = 1;
}

void
ui_set_input_blocked(int blocked)
{
    g_ui_input_blocked = blocked != 0;
}

int
ui_input_captures_click(Vector2 point)
{
    return g_ui_input_blocked || ui_dropdown_captures_click(point);
}

static int
ui_clampi(int value, int min_value, int max_value)
{
    if(value < min_value)
        return min_value;
    if(value > max_value)
        return max_value;
    return value;
}

void
ui_focus_begin(void)
{
    g_ui_focus_count = 0;
    g_ui_focus_tab_dir = 0;
    if(IsKeyPressed(KEY_TAB))
        g_ui_focus_tab_dir = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ? -1 : 1;
}

void
ui_focus_end(void)
{
    int current_index = -1;
    int next_index;

    if(g_ui_focus_count <= 0) {
        g_ui_focus_active_id = 0;
        return;
    }

    if(g_ui_focus_tab_dir == 0)
        return;

    for(int i = 0; i < g_ui_focus_count; i++) {
        if(g_ui_focus_ids[i] == g_ui_focus_active_id) {
            current_index = i;
            break;
        }
    }

    if(current_index < 0)
        next_index = g_ui_focus_tab_dir > 0 ? 0 : g_ui_focus_count - 1;
    else
        next_index = (current_index + g_ui_focus_tab_dir + g_ui_focus_count) % g_ui_focus_count;

    g_ui_focus_active_id = g_ui_focus_ids[next_index];
}

int
ui_focus_register(int id, Rectangle bounds)
{
    Vector2 mouse_world;

    if(id <= 0)
        return 0;

    if(g_ui_focus_count < UI_FOCUS_MAX_ITEMS)
        g_ui_focus_ids[g_ui_focus_count++] = id;

    mouse_world = ui_mouse_world();
    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
       CheckCollisionPointRec(mouse_world, bounds) &&
       !ui_input_captures_click(mouse_world))
        g_ui_focus_active_id = id;

    return g_ui_focus_active_id == id;
}

int
ui_focus_is_active(int id)
{
    return id > 0 && g_ui_focus_active_id == id;
}

int
ui_focus_activate_pressed(int id)
{
    return ui_focus_is_active(id) &&
           (IsKeyPressed(KEY_ENTER) || (!g_ui_focus_text_input_active && IsKeyPressed(KEY_SPACE)));
}

void
ui_focus_set(int id)
{
    g_ui_focus_active_id = id;
}

void
ui_focus_clear(void)
{
    g_ui_focus_active_id = 0;
}

void
ui_focus_set_text_input_active(int active)
{
    g_ui_focus_text_input_active = active;
}

void
ui_focus_draw(Rectangle bounds)
{
    DrawRectangleRoundedLinesEx((Rectangle){bounds.x - flint_px(3), bounds.y - flint_px(3),
                                            bounds.width + flint_px(6), bounds.height + flint_px(6)},
                                0.10f, 10, flint_px(2), c_button_hover);
}

int
flint_ui_font(void)
{
    return flint_px(16);
}

int
flint_ui_font_small(void)
{
    return flint_ui_font();
}

int
flint_ui_text_y(const char *text, int box_y, int box_h, int font)
{
    return flint_text_y(text, box_y, box_h, font);
}

void
flint_ui_draw_text_centered(const char *text, int center_x, int center_y, int font, Color color)
{
    int text_w = flint_text_measure(text, font);
    int y = flint_ui_text_y(text, center_y - font / 2, font, font);

    flint_text_draw(text, center_x - text_w / 2, y, font, color);
}

void
flint_ui_draw_text_input(Rectangle bounds, const char *text, int cursor_position,
                         int focused, int cursor_visible, int font,
                         FlintUITextInputStyle style)
{
    const char *value = text ? text : "";
    int x = (int)bounds.x;
    int y = (int)bounds.y;
    int w = (int)bounds.width;
    int h = (int)bounds.height;
    int padding_x = style.padding_x > 0 ? style.padding_x : flint_px(10);
    int text_x = x + padding_x;
    int text_y = flint_ui_text_y(value, y, h, font);
    Color border = focused ? style.focus_border : style.border;
    float radius = style.radius > 0.0f ? style.radius : 0.12f;

    DrawRectangleRounded(bounds, radius, 8, style.background);
    DrawRectangleRoundedLines(bounds, radius, 8, border);

    ui_begin_scissor(x + padding_x, y, w - padding_x * 2, h);
    flint_text_draw(value, text_x, text_y, font, style.text);

    if(focused && cursor_visible) {
        char before_cursor[1024];
        int len = (int)strlen(value);
        int clamped_cursor = ui_clampi(cursor_position, 0, len);
        int copy_len = clamped_cursor;
        if(copy_len >= (int)sizeof(before_cursor))
            copy_len = (int)sizeof(before_cursor) - 1;
        memcpy(before_cursor, value, (size_t)copy_len);
        before_cursor[copy_len] = '\0';

        int cursor_x = text_x + flint_text_measure(before_cursor, font);
        DrawRectangle(cursor_x, y + flint_px(6), flint_px(2), h - flint_px(12), style.cursor);
    }
    ui_end_scissor();
}

int
flint_ui_button(FlintUIButton button)
{
    Vector2 mouse_world = ui_mouse_world();
    int mouse_inside = CheckCollisionPointRec(mouse_world, button.bounds);
    int captured = ui_input_captures_click(mouse_world);
    int hovered = !button.disabled && !captured && mouse_inside;
    int focused = !button.disabled && button.focus_id > 0 && ui_focus_register(button.focus_id, button.bounds);
    int clicked = 0;
    int font = button.font > 0 ? button.font : flint_ui_font();
    Color background = button.background.a != 0 ? button.background : c_button;
    Color hover_background = button.hover_background.a != 0 ? button.hover_background : c_button_hover;
    Color text = button.text.a != 0 ? button.text : c_text;
    Color border = button.border.a != 0 ? button.border : flint_lighten(background, 32);
    float radius = button.radius > 0.0f ? button.radius : 0.12f;

    if(button.disabled) {
        background.a = background.a > 120 ? 120 : background.a;
        text.a = text.a > 150 ? 150 : text.a;
    }

    DrawRectangleRounded(button.bounds, radius, 8, hovered ? hover_background : background);
    DrawRectangleRoundedLines(button.bounds, radius, 8,
                              hovered ? flint_lighten(hover_background, 40) : border);

    if(button.disabled && !captured && mouse_inside)
        ui_mark_disabled();

    if(hovered) {
        ui_mark_clickable();
        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
            clicked = 1;
    }

    if(focused) {
        ui_focus_set_text_input_active(0);
        ui_focus_draw(button.bounds);
    }

    flint_text_draw_fitted_in_rect(button.label ? button.label : "", button.bounds,
                                   font, FLINT_TEXT_16, text);
    return clicked || ui_focus_activate_pressed(button.focus_id);
}

int
flint_ui_icon_button(FlintUIIconButton button)
{
    Vector2 mouse_world = ui_mouse_world();
    int mouse_inside = CheckCollisionPointRec(mouse_world, button.bounds);
    int captured = ui_input_captures_click(mouse_world);
    int hovered = !button.disabled && !captured && mouse_inside;
    int focused = !button.disabled && button.focus_id > 0 && ui_focus_register(button.focus_id, button.bounds);
    int clicked = 0;
    int icon_padding = button.icon_padding > 0 ? button.icon_padding : flint_px(4);
    int draw_size = button.icon_size;
    Color background = button.background.a != 0 ? button.background : c_button;
    Color hover_background = button.hover_background.a != 0 ? button.hover_background : c_button_hover;
    Color icon_color = button.icon_color.a != 0 ? button.icon_color : c_icon;
    Color border = button.border.a != 0 ? button.border : flint_lighten(background, 32);
    float radius = button.radius > 0.0f ? button.radius : 0.12f;

    if(draw_size <= 0) {
        int available_w = (int)button.bounds.width - icon_padding * 2;
        int available_h = (int)button.bounds.height - icon_padding * 2;
        draw_size = available_w < available_h ? available_w : available_h;
    }
    if(draw_size < 1)
        draw_size = 1;

    if(button.disabled) {
        background.a = background.a > 120 ? 120 : background.a;
        icon_color.a = icon_color.a > 150 ? 150 : icon_color.a;
    }

    DrawRectangleRounded(button.bounds, radius, 8, hovered ? hover_background : background);
    DrawRectangleRoundedLines(button.bounds, radius, 8,
                              hovered ? flint_lighten(hover_background, 40) : border);

    if(button.disabled && !captured && mouse_inside)
        ui_mark_disabled();

    if(hovered) {
        ui_mark_clickable();
        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
            clicked = 1;
    }

    if(focused) {
        ui_focus_set_text_input_active(0);
        ui_focus_draw(button.bounds);
    }

    int icon_x = (int)(button.bounds.x + (button.bounds.width - (float)draw_size) * 0.5f);
    int icon_y = (int)(button.bounds.y + (button.bounds.height - (float)draw_size) * 0.5f);
    if(button.icon.id != 0) {
        Rectangle src = {0, 0, (float)button.icon.width, (float)button.icon.height};
        Rectangle dst = {(float)icon_x, (float)icon_y, (float)draw_size, (float)draw_size};
        DrawTexturePro(button.icon, src, dst, (Vector2){0}, 0, icon_color);
    }
    return clicked || ui_focus_activate_pressed(button.focus_id);
}

int
flint_ui_text_input(FlintUITextInput input)
{
    int focused = input.focused;

    if(input.focus_id > 0 && ui_focus_register(input.focus_id, input.bounds)) {
        focused = 1;
        ui_focus_set_text_input_active(1);
        ui_focus_draw(input.bounds);
    }

    flint_ui_draw_text_input(input.bounds, input.text, input.cursor_position,
                             focused, input.cursor_visible,
                             input.font > 0 ? input.font : flint_ui_font(),
                             input.style);
    return focused;
}

static FlintTextLayout
flint_ui_paragraph_layout(FlintUIParagraph paragraph)
{
    int font = paragraph.font > 0 ? paragraph.font : flint_ui_font();
    int line_gap = paragraph.line_gap > 0 ? paragraph.line_gap : flint_px(4);
    int icon_size = paragraph.icon_size > 0 ? paragraph.icon_size : font;
    FlintTextLayout layout = flint_text_layout_parse(paragraph.text ? paragraph.text : "",
                                                     paragraph.icon,
                                                     paragraph.icon_type,
                                                     icon_size);
    flint_text_layout_reflow(&layout, paragraph.width, font, line_gap);
    return layout;
}

int
flint_ui_paragraph_height(FlintUIParagraph paragraph)
{
    if(paragraph.width <= 0)
        return 0;
    FlintTextLayout layout = flint_ui_paragraph_layout(paragraph);
    int height = flint_text_layout_get_height(&layout);
    flint_text_layout_free(&layout);
    return height;
}

void
flint_ui_paragraph_draw(FlintUIParagraph paragraph, int x, int *y)
{
    if(y == NULL || paragraph.width <= 0)
        return;
    int font = paragraph.font > 0 ? paragraph.font : flint_ui_font();
    FlintTextLayout layout = flint_ui_paragraph_layout(paragraph);
    flint_text_layout_draw(&layout, x, y, font, paragraph.color);
    flint_text_layout_free(&layout);
}

void
flint_ui_draw_bevel(int x, int y, int w, int h, Color light, Color dark)
{
    DrawLine(x, y, x + w - 1, y, light);
    DrawLine(x, y, x, y + h - 1, light);
    DrawLine(x, y + h - 1, x + w - 1, y + h - 1, dark);
    DrawLine(x + w - 1, y, x + w - 1, y + h - 1, dark);
}

void
flint_ui_draw_text_lines(const char **lines, int count, int x, int *y, int font, int line_h, Color color)
{
    for(int i = 0; i < count; i++) {
        flint_text_draw(lines[i], x, *y, font, color);
        *y += line_h;
    }
}

int
flint_ui_icon_btn_size(int size)
{
    switch(size) {
    case UI_ICON_SIZE_TINY: return flint_clamp_px(18, 16, 40);
    case UI_ICON_SIZE_SMALL: return flint_clamp_px(22, 20, 36);
    case UI_ICON_SIZE_MEDIUM: return flint_clamp_px(26, 24, 40);
    case UI_ICON_SIZE_LARGE: return flint_clamp_px(30, 28, 44);
    default: return flint_clamp_px(22, 20, 36);
    }
}

int
flint_ui_icon_btn_padding(int size)
{
    switch(size) {
    case UI_ICON_SIZE_TINY: return flint_px(8);
    case UI_ICON_SIZE_SMALL: return flint_px(10);
    case UI_ICON_SIZE_MEDIUM: return flint_px(12);
    case UI_ICON_SIZE_LARGE: return flint_px(14);
    default: return flint_px(10);
    }
}

void
ui_init(int width, int height, float dpi)
{
    ui_view_width = width;
    ui_view_height = height;
    flint_set_dpi_scale(dpi);
}

void
ui_set_colors(Color text, Color bg, Color surface, Color circle, Color button, Color button_hover, Color icon)
{
    c_text = text;
    c_bg = bg;
    c_surface = surface;
    c_circle = circle;
    c_button = button;
    c_button_hover = button_hover;
    c_icon = icon;
}

void
ui_set_frame(Camera2D camera)
{
    g_ui_camera = camera;
}

void
ui_set_cursor_clickable(int *cursor_clickable)
{
    g_ui_cursor_clickable = cursor_clickable;
}

void
ui_set_cursor_disabled(int *cursor_disabled)
{
    g_ui_cursor_disabled = cursor_disabled;
}

void
ui_set_icons(Texture2D gear_icon, Texture2D x_icon)
{
    g_ui_gear_icon = gear_icon;
    g_ui_x_icon = x_icon;
}

void
ui_draw_bevel(int x, int y, int w, int h, Color light, Color dark)
{
    flint_ui_draw_bevel(x, y, w, h, light, dark);
}

void
ui_draw_text_lines(const char **lines, int count, int x, int *y, int font, int line_h, Color color)
{
    flint_ui_draw_text_lines(lines, count, x, y, font, line_h, color);
}

/* ================================================================
 * ICON BUTTONS
 * ================================================================ */

int
ui_icon_btn_size(UIIconSize size)
{
    return flint_ui_icon_btn_size((int)size);
}

int
ui_icon_btn_padding(UIIconSize size)
{
    return flint_ui_icon_btn_padding((int)size);
}

int
ui_draw_icon_btn(int x, int y, UIIconSize size, Texture2D icon, int *hover)
{
    int btn_size = ui_icon_btn_size(size);
    int padding = ui_icon_btn_padding(size);
    int w = btn_size + padding * 2;
    int h = btn_size + padding * 2;

    Vector2 mouse_world = ui_mouse_world();
    int mx = (int)mouse_world.x;
    int my = (int)mouse_world.y;
    int mb = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    int released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    int pressed = 0;

    if(mx > x && mx < x + w && my > y && my < y + h && !ui_input_captures_click(mouse_world)) {
        DrawRectangle(x, y, w, h, c_button_hover);
        ui_draw_bevel(x, y, w, h, flint_darken(c_button_hover, 40), flint_lighten(c_button_hover, 40));
        *hover = 1;
        ui_mark_clickable();
        if(mb) {
            ui_draw_bevel(x, y, w, h, flint_lighten(c_button_hover, 40), flint_darken(c_button_hover, 40));
        }
        if(released) {
            pressed = 1;
        }
    } else {
        DrawRectangle(x, y, w, h, c_button);
        ui_draw_bevel(x, y, w, h, flint_lighten(c_button, 40), flint_darken(c_button, 40));
        *hover = 0;
    }

    if(icon.id != 0) {
        Rectangle src = {0, 0, icon.width, icon.height};
        Rectangle dst = {x + padding, y + padding, (float)btn_size, (float)btn_size};
        DrawTexturePro(icon, src, dst, (Vector2){0}, 0, c_icon);
    }

    return pressed;
}

int
ui_draw_icon_btn_padded(int x, int y, int size, int padding, Texture2D icon, int *hover)
{
    Vector2 mouse_world = ui_mouse_world();
    int mx = (int)mouse_world.x;
    int my = (int)mouse_world.y;
    int mb = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    int released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    int w = size + padding * 2;
    int h = size + padding * 2;
    int pressed = 0;

    if(mx > x && mx < x + w && my > y && my < y + h && !ui_input_captures_click(mouse_world)) {
        DrawRectangle(x, y, w, h, c_button_hover);
        ui_draw_bevel(x, y, w, h, flint_darken(c_button_hover, 40), flint_lighten(c_button_hover, 40));
        *hover = 1;
        ui_mark_clickable();
        if(mb) {
            ui_draw_bevel(x, y, w, h, flint_lighten(c_button_hover, 40), flint_darken(c_button_hover, 40));
        }
        if(released) {
            pressed = 1;
        }
    } else {
        DrawRectangle(x, y, w, h, c_button);
        ui_draw_bevel(x, y, w, h, flint_lighten(c_button, 40), flint_darken(c_button, 20));
        *hover = 0;
    }

    if(icon.id != 0) {
        Rectangle src = {0, 0, icon.width, icon.height};
        Rectangle dst = {x + padding, y + padding, (float)size, (float)size};
        DrawTexturePro(icon, src, dst, (Vector2){0}, 0, c_icon);
    }

    return pressed;
}

int
ui_draw_text_btn(int x, int y, const char *label, int *hover)
{
    Vector2 mouse_world = ui_mouse_world();
    int mx = (int)mouse_world.x;
    int my = (int)mouse_world.y;

    int mb = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    int released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    int font = flint_clamp_px(20, 16, 22);
    int w = (int)flint_text_measure(label, font) + flint_px(20);
    int h = flint_clamp_px(30, 26, 34);

    x = x - w / 2;

    int pressed = 0;

    if(mx > x && mx < x + w && my > y && my < y + h && !ui_input_captures_click(mouse_world)) {
        DrawRectangle(x, y, w, h, c_button_hover);
        ui_draw_bevel(x, y, w, h, flint_darken(c_button_hover, 40), flint_lighten(c_button_hover, 40));
        *hover = 1;
        ui_mark_clickable();
        if(mb) {
            ui_draw_bevel(x, y, w, h, flint_lighten(c_button_hover, 40), flint_darken(c_button_hover, 40));
        }
        if(released) {
            pressed = 1;
        }
    } else {
        DrawRectangle(x, y, w, h, c_button);
        ui_draw_bevel(x, y, w, h, flint_lighten(c_button, 40), flint_darken(c_button, 40));
        *hover = 0;
    }

    flint_text_draw(label, x + flint_px(10), flint_ui_text_y(label, y, h, font), font, c_text);

    return pressed;
}

int
ui_draw_generic_button(int x, int y, int w, int h, const char *label,
                       UIButtonStyle style, int disabled, int *hover)
{
    Vector2 mouse_world = ui_mouse_world();
    int released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    int font = flint_ui_font_small();

    Color bg, hover_bg, text_color;

    switch(style) {
        case UI_BUTTON_STYLE_PRIMARY:
            bg = c_button;
            hover_bg = c_button_hover;
            text_color = c_text;
            break;

        case UI_BUTTON_STYLE_SECONDARY:
            bg = flint_darken(c_bg, 14);
            hover_bg = c_button;
            text_color = c_text;
            break;

        case UI_BUTTON_STYLE_DANGER:
            bg = (Color){180, 70, 70, 255};
            hover_bg = (Color){200, 90, 90, 255};
            text_color = WHITE;
            break;

        case UI_BUTTON_STYLE_TAB:
            bg = flint_darken(c_bg, 10);
            hover_bg = c_button;
            text_color = c_text;
            break;

        case UI_BUTTON_STYLE_TAB_SELECTED:
            bg = c_button;
            hover_bg = bg;
            text_color = c_text;
            break;

        default:
            bg = c_button;
            hover_bg = c_button_hover;
            text_color = c_text;
            break;
    }

    Rectangle bounds = {x, y, w, h};
    int mouse_inside = CheckCollisionPointRec(mouse_world, bounds);
    int captured = ui_input_captures_click(mouse_world);
    int hovered = mouse_inside && !disabled && !captured;

    if(disabled) {
        bg = flint_darken(bg, 22);
        hover_bg = bg;
        text_color = flint_darken(text_color, 70);
        if(!captured && mouse_inside)
            ui_mark_disabled();
    }

    if(hover != NULL) {
        *hover = hovered;
    }

    int clicked = 0;

    if(hovered) {
        DrawRectangleRec(bounds, hover_bg);
        ui_draw_bevel(x, y, w, h, flint_lighten(hover_bg, 40), flint_darken(hover_bg, 40));
        ui_mark_clickable();

        if(released) {
            clicked = 1;
        }
    } else {
        DrawRectangleRec(bounds, bg);
        ui_draw_bevel(x, y, w, h, flint_lighten(bg, 40), flint_darken(bg, 40));
    }

    int text_w = flint_text_measure(label, font);
    int text_x = x + (w - text_w) / 2;
    int text_y = flint_ui_text_y(label, y, h, font);
    flint_text_draw(label, text_x, text_y, font, text_color);

    return clicked;
}

int
ui_draw_subtab_bar(FlintUISubtabBar bar)
{
    Vector2 mouse_world = ui_mouse_world();
    int released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    int clicked_tab = -1;
    int font = bar.font > 0 ? bar.font : flint_ui_font();
    int tab_w;
    int bar_x = (int)bar.bounds.x;
    int bar_y = (int)bar.bounds.y;
    int bar_w = (int)bar.bounds.width;
    int bar_h = (int)bar.bounds.height;

    if(bar.tabs == NULL || bar.count <= 0 || bar.bounds.width <= 0 || bar.bounds.height <= 0)
        return -1;

    tab_w = bar_w / bar.count;
    if(tab_w <= 0)
        return -1;

    DrawRectangle(bar_x, bar_y, bar_w, bar_h, flint_darken(c_bg, 8));
    DrawLine(bar_x, bar_y, bar_x + bar_w, bar_y, flint_darken(c_bg, 34));
    DrawLine(bar_x, bar_y + bar_h - 1, bar_x + bar_w, bar_y + bar_h - 1, flint_darken(c_bg, 38));

    for(int i = 0; i < bar.count; i++) {
        int tab_x = bar_x + i * tab_w;
        int tab_h = bar_h;
        int is_last = i == bar.count - 1;
        int draw_w = is_last ? bar_x + bar_w - tab_x : tab_w;
        Rectangle tab_rect = {(float)tab_x, bar.bounds.y, (float)draw_w, bar.bounds.height};
        int input_captured = ui_input_captures_click(mouse_world);
        int is_hovered = CheckCollisionPointRec(mouse_world, tab_rect) && !input_captured;
        int is_selected = i == bar.selected_index;
        int is_disabled = bar.tabs[i].disabled;
        Color accent = bar.tabs[i].accent.a != 0 ? bar.tabs[i].accent : c_button_hover;
        Color text_color = c_text;
        const char *label = bar.tabs[i].label ? bar.tabs[i].label : "";

        if(is_disabled) {
            text_color = flint_darken(c_text, 70);
            text_color.a = text_color.a > 150 ? 150 : text_color.a;
        }

        if(is_hovered && !is_disabled && !is_selected)
            DrawRectangle(tab_x, bar_y + flint_px(2), draw_w, tab_h - flint_px(4),
                          flint_darken(c_button_hover, 10));

        if(is_selected) {
            int underline_h = flint_px(3);
            DrawRectangle(tab_x + flint_px(10), bar_y + tab_h - underline_h,
                          draw_w - flint_px(20), underline_h, accent);
        }

        if(i > 0)
            DrawLine(tab_x, bar_y + flint_px(8), tab_x, bar_y + tab_h - flint_px(8),
                     flint_darken(c_bg, 24));

        if(is_hovered) {
            if(is_disabled)
                ui_mark_disabled();
            else if(!is_selected)
                ui_mark_clickable();

            if(released)
                clicked_tab = i;
        }

        flint_text_draw_fitted_in_rect(label, tab_rect, font, FLINT_TEXT_16, text_color);
    }

    return clicked_tab;
}

void
ui_draw_icon_link(int x, int y, int icon_size, Texture2D icon, const char *url)
{
    Vector2 mouse_world = ui_mouse_world();
    int mx = (int)mouse_world.x;
    int my = (int)mouse_world.y;
    int hover = 0;
    int padding = flint_px(4);
    int btn_w = icon_size + padding * 2;
    int btn_h = icon_size + padding * 2;
    int btn_x = x - padding;
    int btn_y = y - padding;

    if(mx > btn_x && mx < btn_x + btn_w && my > btn_y && my < btn_y + btn_h &&
       !ui_input_captures_click(mouse_world)) {
        hover = 1;
        ui_mark_clickable();
    }

    if(hover) {
        DrawRectangle(btn_x, btn_y, btn_w, btn_h, c_button_hover);
        ui_draw_bevel(btn_x, btn_y, btn_w, btn_h, flint_darken(c_button_hover, 40), flint_lighten(c_button_hover, 40));
    } else {
        DrawRectangle(btn_x, btn_y, btn_w, btn_h, c_button);
        ui_draw_bevel(btn_x, btn_y, btn_w, btn_h, flint_lighten(c_button, 40), flint_darken(c_button, 40));
    }

    if(icon.id != 0) {
        Rectangle src = {0, 0, icon.width, icon.height};
        Rectangle dst = {x, y, (float)icon_size, (float)icon_size};
        DrawTexturePro(icon, src, dst, (Vector2){0}, 0, c_icon);
    }

    if(hover && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
#if defined(PLATFORM_WEB)
        EM_ASM({
            window.location.href = UTF8ToString($0);
        }, url);
#else
        OpenURL(url);
#endif
    }
}

/* ================================================================
 * CONTROLS
 * ================================================================ */

static Rectangle
ui_centered_min_hit_rect(int x, int y, int w, int h, int min_w, int min_h)
{
    int hit_w = w < min_w ? min_w : w;
    int hit_h = h < min_h ? min_h : h;

    return (Rectangle){
        (float)(x + w / 2 - hit_w / 2),
        (float)(y + h / 2 - hit_h / 2),
        (float)hit_w,
        (float)hit_h
    };
}

int
ui_draw_slider(int id, int x, int y, int w, const char *label,
               int min, int max, int *value, const char *suffix)
{
    Vector2 mouse_world = ui_mouse_world();
    int mx = (int)mouse_world.x;
    int label_font = flint_ui_font();
    int value_font = flint_ui_font();
    int track_y = y + flint_px(28);
    int track_h = flint_px(8);
    int knob_w = flint_px(12);
    int knob_h = flint_px(22);
    int knob_y = track_y - (knob_h - track_h) / 2;
    int min_touch_h = flint_px(36);
    int changed = 0;
    char value_text[32];
    Rectangle hit = ui_centered_min_hit_rect(x, knob_y, w, knob_h, w, min_touch_h);

    if(g_ui_slider_active_id == id && !IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        g_ui_slider_active_id = 0;

    snprintf(value_text, sizeof(value_text), "%d%s", *value, suffix != NULL ? suffix : "");
    flint_text_draw(label, x, y, label_font, c_text);
    flint_text_draw(value_text, x + w - flint_text_measure(value_text, value_font), y, value_font, c_text);

    DrawRectangle(x, track_y, w, track_h, flint_darken(c_bg, 28));
    ui_draw_bevel(x, track_y, w, track_h, flint_darken(c_bg, 55), flint_lighten(c_bg, 35));

    if(CheckCollisionPointRec(mouse_world, hit) && !ui_input_captures_click(mouse_world)) {
        ui_mark_clickable();
        if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            g_ui_slider_active_id = id;
    }

    if(g_ui_slider_active_id == id && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        int old_value = *value;
        float t = (float)(mx - x) / (float)w;
        if(t < 0.0f)
            t = 0.0f;
        if(t > 1.0f)
            t = 1.0f;
        *value = min + (int)(t * (float)(max - min) + 0.5f);
        *value = ui_clampi(*value, min, max);
        changed = (*value != old_value);
    }

    float t = (float)(*value - min) / (float)(max - min);
    int knob_x = x + (int)(t * (float)w) - knob_w / 2;
    DrawRectangle(knob_x, knob_y, knob_w, knob_h, c_button);
    ui_draw_bevel(knob_x, knob_y, knob_w, knob_h, flint_lighten(c_button, 40), flint_darken(c_button, 40));

    return changed;
}

int
ui_draw_slider_vertical(int id, int x, int y, int h,
                        int min, int max, int *value)
{
    Vector2 mouse_world = ui_mouse_world();
    int my = (int)mouse_world.y;
    int track_w = flint_px(8);
    int knob_w = flint_px(20);
    int knob_h = flint_px(12);
    int track_x = x - track_w / 2;
    int min_touch_w = flint_px(36);
    int changed = 0;
    Rectangle hit = ui_centered_min_hit_rect(x - track_w / 2, y, track_w, h, min_touch_w, h);

    if(g_ui_slider_active_id == id && !IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        g_ui_slider_active_id = 0;

    /* Draw track */
    DrawRectangle(track_x, y, track_w, h, flint_darken(c_bg, 28));
    ui_draw_bevel(track_x, y, track_w, h, flint_darken(c_bg, 55), flint_lighten(c_bg, 35));

    /* Check for interaction */
    if(CheckCollisionPointRec(mouse_world, hit) && !ui_input_captures_click(mouse_world)) {
        ui_mark_clickable();
        if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            g_ui_slider_active_id = id;
    }

    /* Handle drag */
    if(g_ui_slider_active_id == id && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        int old_value = *value;
        /* Invert Y so 0% is at bottom, 100% at top */
        float t = 1.0f - (float)(my - y) / (float)h;
        if(t < 0.0f)
            t = 0.0f;
        if(t > 1.0f)
            t = 1.0f;
        *value = min + (int)(t * (float)(max - min) + 0.5f);
        *value = ui_clampi(*value, min, max);
        changed = (*value != old_value);
    }

    float t = (float)(*value - min) / (float)(max - min);
    int position_y = y + h - (int)(t * (float)h);  /* Position on track */
    int knob_y = position_y - knob_h - flint_px(1);  /* Knob top above position */
    int knob_x = track_x - (knob_w - track_w) / 2;

    if(position_y < y)
        position_y = y;
    if(position_y > y + h)
        position_y = y + h;

    DrawRectangle(track_x, position_y, track_w, y + h - position_y, c_button_hover);
    ui_draw_bevel(track_x, position_y, track_w, y + h - position_y,
                  flint_lighten(c_button_hover, 35), flint_darken(c_button_hover, 35));

    /* Draw knob */
    DrawRectangle(knob_x, knob_y, knob_w, knob_h, c_button);
    ui_draw_bevel(knob_x, knob_y, knob_w, knob_h, flint_lighten(c_button, 40), flint_darken(c_button, 40));

    return changed;
}

int
ui_draw_slider_vertical_with_marks(int id, int x, int y, int h,
                                   int min, int max, int *value, ui_slider_vertical_mark_callback callback,
                                   void *callback_user_data)
{
    Vector2 mouse_world = ui_mouse_world();
    int my = (int)mouse_world.y;
    int track_w = flint_px(8);
    int knob_w = flint_px(20);
    int knob_h = flint_px(12);
    int track_x = x - track_w / 2;
    int min_touch_w = flint_px(36);
    int changed = 0;
    Rectangle hit = ui_centered_min_hit_rect(x - track_w / 2, y, track_w, h, min_touch_w, h);

    if(g_ui_slider_active_id == id && !IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        g_ui_slider_active_id = 0;

    /* Draw track */
    DrawRectangle(track_x, y, track_w, h, flint_darken(c_bg, 28));
    ui_draw_bevel(track_x, y, track_w, h, flint_darken(c_bg, 55), flint_lighten(c_bg, 35));

    /* Draw custom marks via callback (between track and knob) */
    if(callback != NULL)
        callback(callback_user_data, x, y, h, min, max, *value);

    /* Check for interaction */
    if(CheckCollisionPointRec(mouse_world, hit) && !ui_input_captures_click(mouse_world)) {
        ui_mark_clickable();
        if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            g_ui_slider_active_id = id;
    }

    /* Handle drag */
    if(g_ui_slider_active_id == id && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        int old_value = *value;
        /* Invert Y so 0% is at bottom, 100% at top */
        float t = 1.0f - (float)(my - y) / (float)h;
        if(t < 0.0f)
            t = 0.0f;
        if(t > 1.0f)
            t = 1.0f;
        *value = min + (int)(t * (float)(max - min) + 0.5f);
        *value = ui_clampi(*value, min, max);
        changed = (*value != old_value);
    }

    float t = (float)(*value - min) / (float)(max - min);
    int position_y = y + h - (int)(t * (float)h);  /* Position on track */
    int knob_y = position_y - knob_h - flint_px(1);  /* Knob top above position */
    int knob_x = track_x - (knob_w - track_w) / 2;

    if(position_y < y)
        position_y = y;
    if(position_y > y + h)
        position_y = y + h;

    DrawRectangle(track_x, position_y, track_w, y + h - position_y, c_button_hover);
    ui_draw_bevel(track_x, position_y, track_w, y + h - position_y,
                  flint_lighten(c_button_hover, 35), flint_darken(c_button_hover, 35));

    /* Draw knob */
    DrawRectangle(knob_x, knob_y, knob_w, knob_h, c_button);
    ui_draw_bevel(knob_x, knob_y, knob_w, knob_h, flint_lighten(c_button, 40), flint_darken(c_button, 40));

    return changed;
}

int
ui_draw_toggle_switch(int x, int y, int w, int h, int *value,
                     const char *off_label, const char *on_label)
{
    Vector2 mouse_world = ui_mouse_world();
    int hover = 0;
    int min_touch = flint_px(36);
    int font = flint_ui_font();
    int off_w = flint_text_measure(off_label, font);
    int on_w = flint_text_measure(on_label, font);
    int min_half_w = (off_w > on_w ? off_w : on_w) + flint_px(16);
    int min_w = min_half_w * 2 + flint_px(6);
    if(w < min_w)
        w = min_w;
    if(h < flint_px(34))
        h = flint_px(34);
    Rectangle bounds = ui_centered_min_hit_rect(x, y, w, h, min_touch, min_touch);

    if(CheckCollisionPointRec(mouse_world, bounds) && !ui_input_captures_click(mouse_world)) {
        hover = 1;
        ui_mark_clickable();
    }

    int pressed = hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

    if(pressed)
        *value = !(*value);

    Color bg = flint_darken(c_bg, 8);
    DrawRectangle(x, y, w, h, bg);

    int track_h = h - 6;
    int track_y = y + 3;
    DrawRectangleRounded((Rectangle){x + 3, track_y, w - 6, track_h}, 0.5f, 8, flint_darken(c_bg, 20));

    int active_w = (w - 6) / 2;
    int active_x = *value ? x + w - active_w - 3 : x + 3;
    DrawRectangleRounded((Rectangle){active_x, track_y, active_w, track_h}, 0.5f, 8, c_button);

    Color label_color = c_text;
    /* Center text in each half of the toggle */
    int off_x = x + w / 4 - off_w / 2;
    int on_x = x + w * 3 / 4 - on_w / 2;
    flint_text_draw(off_label, off_x, flint_ui_text_y(off_label, y, h, font), font, label_color);
    flint_text_draw(on_label, on_x, flint_ui_text_y(on_label, y, h, font), font, label_color);

    return pressed;
}

int
ui_draw_checkbox_toggle(int x, int y, const char *label, int *value)
{
    int font = flint_ui_font();
    int box_size = flint_px(22);
    int hover = 0;
    Rectangle bounds = {x, y, box_size, box_size};
    Vector2 mouse_world = ui_mouse_world();

    if(CheckCollisionPointRec(mouse_world, bounds) && !ui_input_captures_click(mouse_world)) {
        hover = 1;
        ui_mark_clickable();
    }

    int pressed = hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    if(pressed)
        *value = !(*value);

    DrawRectangle(x, y, box_size, box_size, c_button);
    ui_draw_bevel(x, y, box_size, box_size, flint_darken(c_bg, 30), flint_lighten(c_bg, 20));

    if(*value) {
        int padding = flint_px(4);
        DrawLine(x + padding, y + padding, x + box_size / 2, y + box_size - padding, c_text);
        DrawLine(x + box_size / 2, y + box_size - padding, x + box_size - padding, y + padding, c_text);
    }

    flint_text_draw(label, x + box_size + flint_px(10), flint_ui_text_y(label, y, box_size, font), font, c_text);

    return pressed;
}

/* Per-dropdown state to track open/closed and click handling */
typedef struct UIDropdownState {
    int id;
    int open;
    int just_opened;
    int scroll_offset;
    int x, y, w, h;
    const char **options;
    int option_count;
    int *selected_index;
    int touch_pressed;
    int touch_press_start_y;
    int touch_drag_active;
} UIDropdownState;

#define MAX_DROPDOWNS 8
static UIDropdownState dropdown_states[MAX_DROPDOWNS];
static int dropdown_state_count = 0;
static int dropdown_clip_top = 0;

void
ui_set_dropdown_clip_top(int top)
{
    dropdown_clip_top = top > 0 ? top : 0;
}

static void
dropdown_menu_layout(const UIDropdownState *state, int *dropdown_y, int *dropdown_h,
                     int *visible_options, int *open_up)
{
    int option_h;
    int menu_gap;
    int padding_top;
    int padding_bottom;
    int below_y;
    int below_space;
    int above_space;
    int max_visible_h;
    int total_h;

    if(state == NULL || dropdown_y == NULL || dropdown_h == NULL)
        return;

    option_h = state->h;
    menu_gap = flint_px(4);
    padding_top = flint_px(4);
    padding_bottom = flint_px(4);
    total_h = padding_top + option_h * state->option_count + padding_bottom;
    below_y = state->y + state->h + menu_gap;
    if(below_y < dropdown_clip_top)
        below_y = dropdown_clip_top;
    below_space = ui_view_height - below_y - flint_px(16);
    above_space = state->y - dropdown_clip_top - flint_px(16);

    if(below_space < 0)
        below_space = 0;
    if(above_space < 0)
        above_space = 0;

    if(open_up != NULL)
        *open_up = (above_space > below_space);

    max_visible_h = (above_space > below_space) ? above_space : below_space;
    if(total_h > max_visible_h) {
        int count = (max_visible_h - flint_px(8)) / option_h;
        if(count < 1)
            count = 1;
        total_h = count * option_h + flint_px(8);
        if(visible_options != NULL)
            *visible_options = count;
    } else if(visible_options != NULL) {
        *visible_options = state->option_count;
    }

    if(open_up != NULL && *open_up)
        *dropdown_y = state->y - menu_gap - total_h;
    else
        *dropdown_y = below_y;

    *dropdown_h = total_h;
}

typedef struct {
    int circle_size;
    int label_gap;
    int col_gap;
    int cell_w;
    int per_row;
    int row_width;
    int row_step;
    int row_count;
    int height;
} UIThemeGridLayout;

static UIThemeGridLayout
ui_theme_grid_layout(int w)
{
    UIThemeGridLayout layout = {0};
    int small_font = flint_ui_font_small();

    int row_gap = flint_px(16);
    layout.circle_size = flint_px(36);
    layout.label_gap = flint_px(14);
    layout.col_gap = flint_px(20);
    layout.cell_w = layout.circle_size;
    for(int i = 0; i < FLINT_THEME_COUNT; i++) {
        int name_w = flint_text_measure(flint_theme_label((FlintThemeId)i), small_font) + flint_px(12);
        if(name_w > layout.cell_w)
            layout.cell_w = name_w;
    }

    layout.per_row = 3;
    layout.row_width = layout.per_row * layout.cell_w + (layout.per_row - 1) * layout.col_gap;
    if(layout.row_width > w) {
        layout.per_row = 2;
        layout.row_width = layout.per_row * layout.cell_w + (layout.per_row - 1) * layout.col_gap;
    }
    if(layout.row_width > w) {
        layout.per_row = 1;
        layout.row_width = layout.cell_w;
    }

    layout.row_step = layout.circle_size + layout.label_gap + small_font + row_gap;
    layout.row_count = (FLINT_THEME_COUNT + layout.per_row - 1) / layout.per_row;
    layout.height = layout.circle_size + layout.label_gap + small_font;
    if(layout.row_count > 1)
        layout.height += (layout.row_count - 1) * layout.row_step;

    return layout;
}

static int
ui_draw_theme_grid(int x, int circle_y, int w, int dark, int *theme_id)
{
    int changed = 0;
    int small_font = flint_ui_font_small();
    int selected = theme_id != NULL ? *theme_id : FLINT_THEME_SKY;
    UIThemeGridLayout layout = ui_theme_grid_layout(w);
    int start_x = x + (w - layout.row_width) / 2;
    Vector2 mouse_world = ui_mouse_world();

    if(selected < 0 || selected >= FLINT_THEME_COUNT)
        selected = FLINT_THEME_SKY;

    for(int i = 0; i < FLINT_THEME_COUNT; i++) {
        int row = i / layout.per_row;
        int col = i % layout.per_row;
        int cell_x = start_x + col * (layout.cell_w + layout.col_gap);
        int cx = cell_x + layout.cell_w / 2;
        int cy = circle_y + row * layout.row_step;
        Color theme_color = c_circle;

        if(!flint_theme_catalog_color((FlintThemeId)i, dark != 0, "circle", &theme_color)) {
            const char *scope = flint_theme_scope_for((FlintThemeId)i, dark != 0);
            theme_color = flint_theme_get(scope, "circle");
        }

        DrawCircle(cx, cy, layout.circle_size / 2, theme_color);
        DrawCircleLines(cx, cy, layout.circle_size / 2 + (selected == i ? flint_px(2) : flint_px(1)),
                        selected == i ? c_text : flint_darken(c_bg, 30));

        Rectangle bounds = {
            (float)(cx - layout.circle_size / 2 - flint_px(4)),
            (float)(cy - layout.circle_size / 2 - flint_px(4)),
            (float)(layout.circle_size + flint_px(8)),
            (float)(layout.circle_size + flint_px(8))
        };
        if(CheckCollisionPointRec(mouse_world, bounds) && !ui_input_captures_click(mouse_world)) {
            ui_mark_clickable();
            if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                selected = i;
                if(theme_id != NULL)
                    *theme_id = i;
                changed = 1;
            }
        }

        const char *name = flint_theme_label((FlintThemeId)i);
        int name_w = flint_text_measure(name, small_font);
        flint_text_draw(name, cx - name_w / 2,
                        cy + layout.circle_size / 2 + layout.label_gap,
                        small_font, c_text);
    }

    return changed;
}

int
ui_draw_theme_switcher(int x, int y, int w, const char *label,
                       const char *light_label, const char *dark_label,
                       int *theme_id, int *dark_mode)
{
    int changed = 0;
    int font = flint_ui_font();
    int dark = dark_mode != NULL ? *dark_mode : 0;

    flint_text_draw(label ? label : "Theme", x, y, font, c_text);

    int light_w = flint_text_measure(light_label ? light_label : "Light", font);
    int dark_w = flint_text_measure(dark_label ? dark_label : "Dark", font);
    int max_label_w = light_w > dark_w ? light_w : dark_w;
    int toggle_w = max_label_w * 2 + flint_px(32);
    int min_toggle_w = flint_px(100);
    if(toggle_w < min_toggle_w)
        toggle_w = min_toggle_w;
    if(toggle_w > w)
        toggle_w = w;

    int toggle_h = flint_px(28);
    int toggle_x = x + w - toggle_w - flint_px(8);
    int toggle_y = y - flint_px(2);
    if(ui_draw_toggle_switch(toggle_x, toggle_y, toggle_w, toggle_h, &dark,
                             light_label ? light_label : "Light",
                             dark_label ? dark_label : "Dark")) {
        if(dark_mode != NULL)
            *dark_mode = dark;
        changed = 1;
    }

    if(ui_draw_theme_grid(x, y + flint_px(64), w, dark, theme_id))
        changed = 1;

    return changed;
}

int
ui_draw_theme_picker(int x, int y, int w, const char *label, int dark_mode,
                     int *theme_id)
{
    int changed = 0;
    int font = flint_ui_font();

    flint_text_draw(label ? label : "Theme", x, y, font, c_text);
    if(ui_draw_theme_grid(x, y + flint_px(54), w, dark_mode != 0, theme_id))
        changed = 1;

    return changed;
}

int
ui_theme_picker_height(int w)
{
    return flint_px(54) + ui_theme_grid_layout(w).height;
}

/* Check if an open dropdown should own the current click.
 * Other UI elements should call this so menu clicks and outside-close clicks do
 * not pass through to controls underneath. */
int
ui_dropdown_captures_click(Vector2 point)
{
    for(int i = 0; i < dropdown_state_count; i++) {
        UIDropdownState *state = &dropdown_states[i];
        if(state->open && state->option_count > 0) {
            Rectangle button_bounds = {state->x, state->y, state->w, state->h};
            int dropdown_y = 0;
            int dropdown_h = 0;
            dropdown_menu_layout(state, &dropdown_y, &dropdown_h, NULL, NULL);
            Rectangle menu_bounds = {state->x, dropdown_y, state->w, dropdown_h};
            if(CheckCollisionPointRec(point, button_bounds) ||
               CheckCollisionPointRec(point, menu_bounds) ||
               IsMouseButtonPressed(MOUSE_BUTTON_LEFT) ||
               IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
                return 1;
        }
    }
    return 0;
}

static UIDropdownState *
get_or_create_dropdown_state(int id)
{
    /* Find existing state */
    for(int i = 0; i < dropdown_state_count; i++) {
        if(dropdown_states[i].id == id)
            return &dropdown_states[i];
    }

    /* Create new state */
    if(dropdown_state_count < MAX_DROPDOWNS) {
        dropdown_states[dropdown_state_count].id = id;
        dropdown_states[dropdown_state_count].open = 0;
        dropdown_states[dropdown_state_count].just_opened = 0;
        dropdown_states[dropdown_state_count].scroll_offset = 0;
        dropdown_states[dropdown_state_count].options = NULL;
        dropdown_states[dropdown_state_count].option_count = 0;
        dropdown_states[dropdown_state_count].selected_index = NULL;
        dropdown_states[dropdown_state_count].touch_pressed = 0;
        dropdown_states[dropdown_state_count].touch_press_start_y = 0;
        dropdown_states[dropdown_state_count].touch_drag_active = 0;
        return &dropdown_states[dropdown_state_count++];
    }

    /* Fallback - use first slot */
    dropdown_states[0].id = id;
    return &dropdown_states[0];
}

int
ui_draw_dropdown_button(int id, int x, int y, int w, int h,
                        const char **options, int option_count, int *selected_index)
{
    UIDropdownState *state = get_or_create_dropdown_state(id);
    int font = flint_ui_font();
    int arrow_pad = flint_px(24);
    int arrow_size = flint_px(6);
    int changed = 0;
    Rectangle btn_bounds = {x, y, w, h};
    Vector2 mouse = ui_mouse_world();
    int hover = CheckCollisionPointRec(mouse, btn_bounds);

    /* Calculate arrow position */
    int arrow_x = x + w - arrow_pad;
    int arrow_y = y + h / 2;

    /* Store state for menu drawing */
    state->x = x;
    state->y = y;
    state->w = w;
    state->h = h;
    state->options = options;
    state->option_count = option_count;
    state->selected_index = selected_index;

    if(hover)
        ui_mark_clickable();

    /* Handle click on button */
    if(hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        state->open = !state->open;
        if(state->open) {
            state->just_opened = 1;
            state->scroll_offset = 0;
            state->touch_drag_active = 0;
        }
    }

    /* Draw button background */
    DrawRectangleRounded(btn_bounds, 0.3f, 8, flint_darken(c_bg, 8));

    /* Draw current selection text, clipped before the X icon. */
    const char *current_name = options[*selected_index];
    int text_x = x + flint_px(12);
    int text_w = arrow_x - arrow_size - flint_px(8) - text_x;
    if(text_w > 0) {
        ui_begin_scissor((int)(g_ui_camera.offset.x + (float)text_x * g_ui_camera.zoom),
                         (int)(g_ui_camera.offset.y + (float)y * g_ui_camera.zoom),
                         (int)((float)text_w * g_ui_camera.zoom),
                         (int)((float)h * g_ui_camera.zoom));
        flint_text_draw(current_name, text_x, flint_ui_text_y(current_name, y, h, font), font, c_text);
        ui_end_scissor();
    }

    /* Draw dropdown X icon */
    int x_size = arrow_size;
    int x_half = x_size / 2;
    int x1 = arrow_x - x_half;
    int x2 = arrow_x + x_half;
    int y1 = arrow_y - x_half;
    int y2 = arrow_y + x_half;
    DrawLine(x1, y1, x2, y2, BLACK);
    DrawLine(x1, y2, x2, y1, BLACK);

    return changed;
}

int
ui_draw_dropdown_menu(int id)
{
    UIDropdownState *state = get_or_create_dropdown_state(id);
    int changed = 0;

    if(!state->open || state->options == NULL || state->selected_index == NULL)
        return 0;

    int font = flint_ui_font();
    int x = state->x;
    int y = state->y;
    int w = state->w;
    int h = state->h;
    int option_h = h;
    int option_count = state->option_count;
    int *selected_index = state->selected_index;
    const char **options = state->options;

    int dropdown_y = 0;
    int dropdown_h = 0;
    int padding_top = flint_px(4);
    int padding_bottom = flint_px(4);
    int content_h = padding_top + option_h * option_count + padding_bottom;
    int max_scroll;
    int scrollbar_w = flint_px(8);
    int option_w = w;

    dropdown_menu_layout(state, &dropdown_y, &dropdown_h, NULL, NULL);
    max_scroll = content_h - dropdown_h;
    if(max_scroll < 0)
        max_scroll = 0;
    if(state->scroll_offset > max_scroll)
        state->scroll_offset = max_scroll;
    if(state->scroll_offset < 0)
        state->scroll_offset = 0;
    if(max_scroll > 0)
        option_w = w - scrollbar_w - flint_px(2);

    Rectangle menu_bounds = {x, dropdown_y, w, dropdown_h};
    Rectangle btn_bounds = {x, y, w, h};
    Vector2 mouse = ui_mouse_world();
    int my = (int)mouse.y;
    int pointer_in_dropdown = CheckCollisionPointRec(mouse, btn_bounds) ||
                              CheckCollisionPointRec(mouse, menu_bounds);

    /* Track pointer movement to distinguish click from drag */
    if(IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        if(!state->touch_pressed && pointer_in_dropdown) {
            /* Pointer just went down - reset drag state */
            state->touch_pressed = 1;
            state->touch_press_start_y = my;
            state->touch_drag_active = 0;
        } else if(!state->touch_drag_active) {
            /* Check if movement exceeded threshold (making it a drag, not a click) */
            int dy = my - state->touch_press_start_y;
            if(abs(dy) > flint_px(8)) {  /* 8px threshold */
                state->touch_drag_active = 1;
            }
        }

        /* If dragging, scroll the dropdown */
        if(state->touch_drag_active && max_scroll > 0) {
            int dy = my - state->touch_press_start_y;
            state->scroll_offset -= dy;
            /* Update start position for continuous scrolling */
            state->touch_press_start_y = my;

            /* Clamp scroll offset */
            if(state->scroll_offset < 0)
                state->scroll_offset = 0;
            if(state->scroll_offset > max_scroll)
                state->scroll_offset = max_scroll;
        }
    } else if(state->touch_pressed) {
        /* Pointer just released - only reset touch_pressed, keep touch_drag_active for selection check */
        state->touch_pressed = 0;
    }

    /* Click outside closes dropdown */
    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if(!state->just_opened &&
           !CheckCollisionPointRec(mouse, btn_bounds) &&
           !CheckCollisionPointRec(mouse, menu_bounds)) {
            state->open = 0;
            state->touch_drag_active = 0;
            state->touch_pressed = 0;
        }
    }

    if(state->just_opened && !IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        state->just_opened = 0;

    if(CheckCollisionPointRec(mouse, menu_bounds)) {
        float wheel = GetMouseWheelMove();
        if(wheel != 0.0f && max_scroll > 0) {
            state->scroll_offset -= (int)(wheel * (float)option_h);
            if(state->scroll_offset < 0)
                state->scroll_offset = 0;
            if(state->scroll_offset > max_scroll)
                state->scroll_offset = max_scroll;
        }
    }

    /* Draw dropdown background */
    DrawRectangle(x, dropdown_y, w, dropdown_h, c_button);
    ui_draw_bevel(x, dropdown_y, w, dropdown_h, flint_darken(c_bg, 30), flint_lighten(c_bg, 20));

    ui_begin_scissor((int)(g_ui_camera.offset.x + (float)x * g_ui_camera.zoom),
                     (int)(g_ui_camera.offset.y + (float)dropdown_y * g_ui_camera.zoom),
                     (int)((float)w * g_ui_camera.zoom),
                     (int)((float)dropdown_h * g_ui_camera.zoom));

    /* Draw options */
    for(int i = 0; i < option_count; i++) {
        int option_y = dropdown_y + padding_top + i * option_h - state->scroll_offset;
        Rectangle option_bounds = {x, option_y, option_w, option_h};

        /* Skip if outside visible area - use inclusive bounds for last item */
        if(option_y + option_h < dropdown_y || option_y >= dropdown_y + dropdown_h)
            continue;

        int option_hover = CheckCollisionPointRec(mouse, option_bounds);

        if(option_hover) {
            DrawRectangle(x, option_y, w, option_h, c_button_hover);
            ui_mark_clickable();

            if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && !state->just_opened && !state->touch_drag_active) {
                *selected_index = i;
                state->open = 0;
                state->just_opened = 0;
                state->touch_drag_active = 0;
                state->touch_pressed = 0;
                state->scroll_offset = 0;
                changed = 1;
                ui_end_scissor();
                goto draw_arrow;
            }
        }

        flint_text_draw(options[i], x + flint_px(12), flint_ui_text_y(options[i], option_y, option_h, font), font, c_text);
    }

    ui_end_scissor();

    if(max_scroll > 0)
        ui_draw_scrollbar(x + w - scrollbar_w, dropdown_y + flint_px(2),
                          dropdown_h - flint_px(4), content_h, &state->scroll_offset, max_scroll);

draw_arrow:
    ;

    /* Redraw arrow on top of everything */
    int arrow_pad = flint_px(24);
    int arrow_size = flint_px(6);
    int arrow_x = x + w - arrow_pad;
    int arrow_y = y + h / 2;

    /* Draw dropdown X icon */
    int x_size = arrow_size;
    int x_half = x_size / 2;
    int x1 = arrow_x - x_half;
    int x2 = arrow_x + x_half;
    int y1 = arrow_y - x_half;
    int y2 = arrow_y + x_half;
    DrawLine(x1, y1, x2, y2, c_text);
    DrawLine(x1, y2, x2, y1, c_text);
    return changed;
}

int
ui_nav_button_width(const char *label, int icon_size, int show_label, int font)
{
    int padding = flint_px(6);
    int width = icon_size + padding * 2;

    if(show_label && label != NULL && label[0] != '\0')
        width += flint_px(10) + flint_text_measure(label, font);
    return width;
}

int
ui_draw_nav_button(int x, int y, int icon_size, Texture2D icon,
                   const char *label, int show_label, int *hover)
{
    Vector2 mouse_world = ui_mouse_world();
    int mx = (int)mouse_world.x;
    int my = (int)mouse_world.y;
    int mb = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    int released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    int font = flint_ui_font();
    int padding = flint_px(6);
    int w = ui_nav_button_width(label, icon_size, show_label, font);
    int h = icon_size + padding * 2;
    int pressed = 0;

    if(mx > x && mx < x + w && my > y && my < y + h) {
        DrawRectangle(x, y, w, h, c_button_hover);
        ui_draw_bevel(x, y, w, h, flint_darken(c_button_hover, 40), flint_lighten(c_button_hover, 40));
        *hover = 1;
        ui_mark_clickable();
        if(mb)
            ui_draw_bevel(x, y, w, h, flint_lighten(c_button_hover, 40), flint_darken(c_button_hover, 40));
        if(released)
            pressed = 1;
    } else {
        DrawRectangle(x, y, w, h, c_button);
        ui_draw_bevel(x, y, w, h, flint_lighten(c_button, 40), flint_darken(c_button, 40));
        *hover = 0;
    }

    if(icon.id != 0) {
        Rectangle src = {0, 0, icon.width, icon.height};
        /* Center icon horizontally when no label, otherwise align left */
        int icon_x = show_label && label && label[0] ? x + padding : x + (w - icon_size) / 2;
        Rectangle dst = {icon_x, y + padding, (float)icon_size, (float)icon_size};
        DrawTexturePro(icon, src, dst, (Vector2){0}, 0, c_icon);
    }

    if(show_label && label != NULL && label[0] != '\0') {
        int text_x = x + icon_size + padding * 2 + flint_px(10);
        flint_text_draw(label, text_x, flint_ui_text_y(label, y, h, font), font, c_text);
    }

    return pressed;
}

int
ui_draw_nav_button_expand(int x, int y, int icon_size, int w, Texture2D icon,
                           const char *label, int show_label, int *hover)
{
    Vector2 mouse_world = ui_mouse_world();
    int mx = (int)mouse_world.x;
    int my = (int)mouse_world.y;
    int mb = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    int released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    int font = flint_ui_font();
    int padding = flint_px(6);
    int h = icon_size + padding * 2;
    int pressed = 0;

    if(mx > x && mx < x + w && my > y && my < y + h) {
        DrawRectangle(x, y, w, h, c_button_hover);
        ui_draw_bevel(x, y, w, h, flint_darken(c_button_hover, 40), flint_lighten(c_button_hover, 40));
        *hover = 1;
        ui_mark_clickable();
        if(mb)
            ui_draw_bevel(x, y, w, h, flint_lighten(c_button_hover, 40), flint_darken(c_button_hover, 40));
        if(released)
            pressed = 1;
    } else {
        DrawRectangle(x, y, w, h, c_button);
        ui_draw_bevel(x, y, w, h, flint_lighten(c_button, 40), flint_darken(c_button, 40));
        *hover = 0;
    }

    if(icon.id != 0) {
        Rectangle src = {0, 0, icon.width, icon.height};
        /* Center icon horizontally when no label, otherwise align left */
        int icon_x = show_label && label && label[0] ? x + padding : x + (w - icon_size) / 2;
        Rectangle dst = {icon_x, y + padding, (float)icon_size, (float)icon_size};
        DrawTexturePro(icon, src, dst, (Vector2){0}, 0, c_icon);
    }

    if(show_label && label != NULL && label[0] != '\0') {
        int text_x = x + icon_size + padding * 2 + flint_px(10);
        flint_text_draw(label, text_x, flint_ui_text_y(label, y, h, font), font, c_text);
    }

    return pressed;
}

/* ================================================================
 * TAB BAR
 * ================================================================ */

void
ui_draw_tab_bar(UITab *tabs, int count)
{
    int bar_h = flint_clamp_px(58, 54, 66);
    int bar_y = ui_view_height - bar_h;
    int button_size = flint_clamp_px(30, 28, 44);
    int button_h = button_size + flint_px(12);
    int font = flint_ui_font();
    int side_margin = flint_px(16);
    int group_gap = flint_px(10);
    int available_w = ui_view_width - side_margin * 2;
    int label_safety_w = group_gap * (count + 1);

    /* Calculate widths with labels */
    int group_w_label = 0;
    for(int i = 0; i < count; i++) {
        group_w_label += ui_nav_button_width(tabs[i].label, button_size, 1, font);
        if(i < count - 1)
            group_w_label += group_gap;
    }

    /* Calculate widths without labels */
    int group_w_no_label = 0;
    for(int i = 0; i < count; i++) {
        group_w_no_label += ui_nav_button_width(tabs[i].label, button_size, 0, font);
        if(i < count - 1)
            group_w_no_label += group_gap;
    }

    /* Only show labels if all buttons fit with breathing room for longer locales. */
    int show_labels = group_w_label + label_safety_w <= available_w;
    int base_group_w = show_labels ? group_w_label : group_w_no_label;

    /* Calculate extra space and distribute evenly among buttons */
    int extra_w = available_w - base_group_w;
    if(extra_w < 0)
        extra_w = 0;
    int extra_per_button = count > 0 ? extra_w / count : 0;
    int remainder = count > 0 ? extra_w % count : 0;

    /* Calculate total width with extra space included */
    int total_w = base_group_w + extra_per_button * count;

    /* Center the tab group - remainder adds extra left margin for true centering */
    int group_x = side_margin + (available_w - total_w) / 2 + remainder / 2;
    int button_y = bar_y + (bar_h - button_h) / 2;
    int tab_hover = 0;

    DrawRectangle(0, bar_y, ui_view_width, bar_h, flint_darken(c_bg, 10));
    DrawLine(0, bar_y, ui_view_width, bar_y, flint_darken(c_bg, 42));

    int x = group_x;
    for(int i = 0; i < count; i++) {
        int base_w = ui_nav_button_width(tabs[i].label, button_size, show_labels, font);
        int w = base_w + extra_per_button;

        if(ui_draw_nav_button_expand(x, button_y, button_size, w, tabs[i].icon,
                                        tabs[i].label, show_labels, &tab_hover)) {
            if(tabs[i].on_click)
                tabs[i].on_click(tabs[i].user_data);
        }

        x += w + group_gap;
    }
}

/* ================================================================
 * TUTORIAL HELPERS
 * ================================================================ */

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

    DrawRectangle(x, y, w, h, flint_darken(c_bg, 12));
    ui_draw_bevel(x, y, w, h, flint_darken(c_bg, 45), flint_lighten(c_bg, 35));
    DrawTexturePro(texture, src, dst, (Vector2){0}, 0, WHITE);
}

/* ================================================================
 * MODAL DIALOGS
 * ================================================================ */

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
    flint_text_layout_reflow(&msg_layout, msg_w, msg_font, msg_font + flint_px(4));

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
        DrawRectangle(confirm_x, btn_y, btn_w, btn_h, c_circle);
        ui_draw_bevel(confirm_x, btn_y, btn_w, btn_h, flint_lighten(c_circle, 40), flint_darken(c_circle, 40));
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
    flint_text_layout_reflow(&msg_layout, msg_w, msg_font, msg_font + flint_px(4));

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
        DrawRectangle(middle_x, btn_y, btn_w, btn_h, c_circle);
        ui_draw_bevel(middle_x, btn_y, btn_w, btn_h, flint_lighten(c_circle, 40), flint_darken(c_circle, 40));
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
    int icon_size = flint_px(24);
    int icon_padding = flint_px(8);
    int icon_w = icon_size + icon_padding * 2;
    int title_font = flint_px(22);
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
    int title_font = flint_px(18);
    int icon_size = flint_px(22);
    int icon_padding = flint_px(8);
    int icon_w = icon_size + icon_padding * 2;
    int title_w = flint_text_measure(title, title_font);
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
ui_scrollbar_reserved_width(int max_scroll)
{
    (void)max_scroll;
    return 0;
}

int
ui_scrollbar_content_width(int content_width, int max_scroll)
{
    int reserved = ui_scrollbar_reserved_width(max_scroll);

    if(reserved <= 0)
        return content_width;
    if(content_width <= reserved)
        return 0;
    return content_width - reserved;
}

FlintUIScrollView
ui_scroll_container_begin(FlintUIScrollArea area)
{
    FlintUIScrollView view;
    Vector2 mouse_world = ui_mouse_world();
    int wheel_step = area.wheel_step > 0 ? area.wheel_step : flint_px(42);
    int x = (int)area.bounds.x;
    int y = (int)area.bounds.y;
    int w = (int)area.bounds.width;
    int h = (int)area.bounds.height;

    memset(&view, 0, sizeof(view));
    view.content_x = x;
    view.viewport_h = h;
    view.content_h = area.content_height > 0 ? area.content_height : 0;
    view.max_scroll = view.content_h - h;
    if(view.max_scroll < 0)
        view.max_scroll = 0;

    if(area.scroll_offset != NULL) {
        if(*area.scroll_offset < 0)
            *area.scroll_offset = 0;
        if(*area.scroll_offset > view.max_scroll)
            *area.scroll_offset = view.max_scroll;

        if(view.max_scroll > 0 &&
           CheckCollisionPointRec(mouse_world, area.bounds) &&
           !ui_input_captures_click(mouse_world)) {
            float wheel = GetMouseWheelMove();
            if(wheel != 0.0f) {
                *area.scroll_offset -= (int)(wheel * (float)wheel_step);
                if(*area.scroll_offset < 0)
                    *area.scroll_offset = 0;
                if(*area.scroll_offset > view.max_scroll)
                    *area.scroll_offset = view.max_scroll;
            }
        }
        view.content_y = y - *area.scroll_offset;
    } else {
        view.content_y = y;
    }

    view.content_w = ui_scrollbar_content_width(w, view.max_scroll);
    ui_begin_scissor((int)(g_ui_camera.offset.x + area.bounds.x * g_ui_camera.zoom),
                     (int)(g_ui_camera.offset.y + area.bounds.y * g_ui_camera.zoom),
                     (int)(area.bounds.width * g_ui_camera.zoom),
                     (int)(area.bounds.height * g_ui_camera.zoom));
    return view;
}

void
ui_scroll_container_end(FlintUIScrollArea area, FlintUIScrollView view)
{
    int scrollbar_w = flint_px(8);
    int scrollbar_x;

    ui_end_scissor();

    if(area.scroll_offset == NULL || view.max_scroll <= 0)
        return;

    scrollbar_x = area.scrollbar_x > 0
                      ? area.scrollbar_x
                      : ui_view_width - scrollbar_w;
    ui_draw_scrollbar(scrollbar_x,
                      (int)area.bounds.y,
                      (int)area.bounds.height,
                      view.content_h,
                      area.scroll_offset,
                      view.max_scroll);
}

int
ui_draw_screen_header(const char *title, int show_close)
{
    (void)c_bg; /* unused */
    int title_h = ui_screen_header_height();
    int title_font = flint_ui_font();
    int close_hover = 0;
    int close_clicked = 0;

    /* Draw header background */
    DrawRectangle(0, 0, ui_view_width, title_h, flint_darken(c_bg, 14));
    DrawLine(0, title_h - 1, ui_view_width, title_h - 1, flint_darken(c_bg, 42));

    /* Draw close button if requested */
    int close_x = ui_view_width - flint_px(40) - ui_icon_btn_padding(UI_ICON_SIZE_TINY);
    int title_x = flint_px(16);
    int title_max_w = show_close ? close_x - title_x - flint_px(12) : ui_view_width - title_x * 2;
    while(title_font > flint_px(14) && flint_text_measure(title, title_font) > title_max_w)
        title_font--;
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

int
ui_draw_scrollbar(int x, int y, int viewport_h, int content_h, int *scroll_offset, int max_scroll)
{
    /* Don't show scrollbar if no scrolling needed */
    if(max_scroll <= 0)
        return 0;

    static int scrollbar_drag_active = 0;
    static int scrollbar_drag_start_y = 0;
    static int scrollbar_drag_start_scroll = 0;

    int scrollbar_width = flint_px(8);
    int scrollbar_min_thumb = flint_px(24);
    int track_padding = flint_px(2);

    /* Calculate thumb size and position */
    float content_ratio = (float)viewport_h / (float)content_h;
    int thumb_height = (int)(viewport_h * content_ratio);
    if(thumb_height < scrollbar_min_thumb)
        thumb_height = scrollbar_min_thumb;
    if(thumb_height > viewport_h)
        thumb_height = viewport_h;

    float scroll_ratio = max_scroll > 0 ? (float)*scroll_offset / (float)max_scroll : 0.0f;
    int thumb_y = y + (int)(scroll_ratio * (viewport_h - thumb_height));

    Vector2 mouse_pos = ui_mouse_world();
    int my = (int)mouse_pos.y;
    Rectangle thumb_bounds = {x + track_padding, thumb_y, scrollbar_width - track_padding * 2, thumb_height};
    int input_captured = ui_input_captures_click(mouse_pos);
    int thumb_hover = CheckCollisionPointRec(mouse_pos, thumb_bounds) && !input_captured;

    /* Handle drag state */
    if(IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !input_captured) {
        if(!scrollbar_drag_active) {
            /* Start drag if clicking on thumb */
            if(thumb_hover) {
                scrollbar_drag_active = 1;
                scrollbar_drag_start_y = my;
                scrollbar_drag_start_scroll = *scroll_offset;
            }
        } else {
            /* Continue drag */
            int dy = my - scrollbar_drag_start_y;
            float scroll_per_pixel = (float)max_scroll / (float)(viewport_h - thumb_height);
            int new_scroll = scrollbar_drag_start_scroll + (int)(dy * scroll_per_pixel);
            *scroll_offset = new_scroll;
            if(*scroll_offset < 0) *scroll_offset = 0;
            if(*scroll_offset > max_scroll) *scroll_offset = max_scroll;
        }
    } else {
        scrollbar_drag_active = 0;
    }

    /* Draw track */
    DrawRectangle(x, y, scrollbar_width, viewport_h, flint_darken(c_bg, 20));

    /* Draw thumb */
    Color thumb_color = thumb_hover || scrollbar_drag_active ? c_button_hover : flint_lighten(c_button, 20);
    DrawRectangleRec(thumb_bounds, thumb_color);

    /* Draw bevel on thumb */
    ui_draw_bevel((int)thumb_bounds.x, (int)thumb_bounds.y,
                  (int)thumb_bounds.width, (int)thumb_bounds.height,
                  flint_darken(thumb_color, 40), flint_lighten(thumb_color, 40));

    return 1;
}

/* ================================================================
 * END OF UI FUNCTIONS
 * ================================================================ */
