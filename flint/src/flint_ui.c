#include "flint_ui.h"
#include "platform.h"
#include "ui/ui.h"
#include "flint_clip.h"
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
Color c_text, c_bg, c_surface, c_circle, c_button, c_button_hover, c_icon;
Camera2D g_ui_camera;
static int *g_ui_cursor_clickable = NULL;
static int *g_ui_cursor_disabled = NULL;
Texture2D g_ui_gear_icon = {0};
Texture2D g_ui_x_icon = {0};
int g_ui_slider_active_id = 0;
int g_ui_input_blocked = 0;
static int g_ui_pointer_down = 0;
int g_ui_pointer_dragging = 0;
static int g_ui_pointer_dragged_this_click = 0;
static int g_ui_pointer_start_x = 0;
static int g_ui_pointer_start_y = 0;

int g_ui_pointer_owner = UI_POINTER_OWNER_NONE;

#define UI_FOCUS_MAX_ITEMS 256
static int g_ui_focus_active_id = 0;
static int g_ui_focus_ids[UI_FOCUS_MAX_ITEMS];
static int g_ui_focus_count = 0;
static int g_ui_focus_tab_dir = 0;
static int g_ui_focus_text_input_active = 0;
static int g_ui_platform_text_input_active = 0;
static FlintUITextInputPlatformCallback g_ui_text_input_platform_callback = NULL;

#define UI_TEXT_INPUT_QUEUE_MAX 64
static int g_ui_text_input_codepoints[UI_TEXT_INPUT_QUEUE_MAX];
static int g_ui_text_input_codepoint_count = 0;
static int g_ui_text_input_backspace_count = 0;
static int g_ui_text_input_enter_count = 0;
static double g_ui_backspace_next_repeat_at = 0.0;

#define UI_INPUT_CLIP_STACK_MAX 16
static Rectangle g_ui_input_clip_stack[UI_INPUT_CLIP_STACK_MAX];
static int g_ui_input_clip_stack_count = 0;

Vector2
ui_mouse_world(void)
{
    return GetScreenToWorld2D(GetMousePosition(), g_ui_camera);
}

void
ui_mark_clickable(void)
{
    if(g_ui_cursor_clickable != NULL)
        *g_ui_cursor_clickable = 1;
}

void
ui_mark_disabled(void)
{
    if(g_ui_cursor_disabled != NULL)
        *g_ui_cursor_disabled = 1;
}

static int
ui_iabs(int value)
{
    return value < 0 ? -value : value;
}

static int
ui_pointer_dx(void)
{
    Vector2 mouse = GetMousePosition();
    return (int)mouse.x - g_ui_pointer_start_x;
}

static int
ui_backspace_repeat_count(void)
{
    double now;
    int count = 0;

    if(IsKeyPressed(KEY_BACKSPACE)) {
        g_ui_backspace_next_repeat_at = GetTime() + 0.34;
        return 1;
    }
    if(!IsKeyDown(KEY_BACKSPACE)) {
        g_ui_backspace_next_repeat_at = 0.0;
        return 0;
    }
    now = GetTime();
    if(g_ui_backspace_next_repeat_at <= 0.0) {
        g_ui_backspace_next_repeat_at = now + 0.34;
        return 0;
    }
    while(now >= g_ui_backspace_next_repeat_at && count < 8) {
        count++;
        g_ui_backspace_next_repeat_at += 0.045;
    }
    return count;
}

static int
ui_pointer_dy(void)
{
    Vector2 mouse = GetMousePosition();
    return (int)mouse.y - g_ui_pointer_start_y;
}

int
ui_pointer_drag_is_horizontal(void)
{
    return ui_iabs(ui_pointer_dx()) >= ui_iabs(ui_pointer_dy());
}

static void
ui_update_pointer_gesture(void)
{
    Vector2 mouse = GetMousePosition();
    int mx = (int)mouse.x;
    int my = (int)mouse.y;
    int drag_threshold = flint_px(5);

    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        g_ui_pointer_down = 1;
        g_ui_pointer_dragging = 0;
        g_ui_pointer_dragged_this_click = 0;
        g_ui_pointer_owner = UI_POINTER_OWNER_NONE;
        g_ui_pointer_start_x = mx;
        g_ui_pointer_start_y = my;
    } else if(IsMouseButtonDown(MOUSE_BUTTON_LEFT) && g_ui_pointer_down) {
        int dx = ui_pointer_dx();
        int dy = ui_pointer_dy();
        if(dx > drag_threshold || dx < -drag_threshold ||
           dy > drag_threshold || dy < -drag_threshold) {
            g_ui_pointer_dragging = 1;
            g_ui_pointer_dragged_this_click = 1;
        }
    } else if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        g_ui_pointer_down = 0;
        g_ui_pointer_dragging = 0;
        g_ui_pointer_owner = UI_POINTER_OWNER_NONE;
    } else if(!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        g_ui_pointer_down = 0;
        g_ui_pointer_dragging = 0;
        g_ui_pointer_dragged_this_click = 0;
        g_ui_pointer_owner = UI_POINTER_OWNER_NONE;
    }
}

void
ui_set_input_blocked(int blocked)
{
    g_ui_input_blocked = blocked != 0;
}

int
ui_base_input_captures_click(Vector2 point, int include_pointer_drag)
{
    if(g_ui_input_clip_stack_count > 0 &&
       !CheckCollisionPointRec(point, g_ui_input_clip_stack[g_ui_input_clip_stack_count - 1]))
        return 1;
    return g_ui_input_blocked ||
           (include_pointer_drag && g_ui_pointer_dragging) ||
           (include_pointer_drag &&
            IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
            g_ui_pointer_dragged_this_click);
}

int
ui_input_captures_click_internal(Vector2 point, int include_pointer_drag)
{
    return ui_base_input_captures_click(point, include_pointer_drag) ||
           ui_dropdown_captures_click(point);
}

int
ui_input_captures_click(Vector2 point)
{
    return ui_input_captures_click_internal(point, 1);
}

int
ui_hover_effects_enabled(void)
{
#if INBE_ANDROID_BUILD
    return 0;
#else
    return !g_ui_input_blocked;
#endif
}

void
ui_push_input_clip(Rectangle bounds)
{
    if(g_ui_input_clip_stack_count > 0)
        bounds = flint_clip_intersection(g_ui_input_clip_stack[g_ui_input_clip_stack_count - 1],
                                         bounds);
    if(g_ui_input_clip_stack_count < UI_INPUT_CLIP_STACK_MAX)
        g_ui_input_clip_stack[g_ui_input_clip_stack_count++] = bounds;
}

void
ui_pop_input_clip(void)
{
    if(g_ui_input_clip_stack_count > 0)
        g_ui_input_clip_stack_count--;
}

int
ui_clampi(int value, int min_value, int max_value)
{
    if(value < min_value)
        return min_value;
    if(value > max_value)
        return max_value;
    return value;
}

static int
ui_utf8_next_offset(const char *text, int offset)
{
    int codepoint_size = 0;
    int len;

    if(text == NULL)
        return 0;
    len = (int)strlen(text);
    if(offset < 0)
        offset = 0;
    if(offset >= len)
        return len;

    GetCodepointNext(text + offset, &codepoint_size);
    if(codepoint_size <= 0)
        codepoint_size = 1;
    if(offset + codepoint_size > len)
        return len;
    return offset + codepoint_size;
}

static int
ui_utf8_prev_offset(const char *text, int offset)
{
    int len;

    if(text == NULL)
        return 0;
    len = (int)strlen(text);
    if(offset > len)
        offset = len;
    if(offset <= 0)
        return 0;

    offset--;
    while(offset > 0 && (((unsigned char)text[offset] & 0xC0) == 0x80))
        offset--;
    return offset;
}

static int
ui_utf8_codepoint_count(const char *text)
{
    int count = 0;

    if(text == NULL)
        return 0;
    for(int i = 0; text[i] != '\0';) {
        int next = ui_utf8_next_offset(text, i);
        if(next <= i)
            break;
        count++;
        i = next;
    }
    return count;
}

static int
ui_utf8_encode(int codepoint, char out[5])
{
    if(out == NULL)
        return 0;
    if(codepoint < 0x80) {
        out[0] = (char)codepoint;
        out[1] = '\0';
        return 1;
    }
    if(codepoint < 0x800) {
        out[0] = (char)(0xC0 | (codepoint >> 6));
        out[1] = (char)(0x80 | (codepoint & 0x3F));
        out[2] = '\0';
        return 2;
    }
    if(codepoint < 0x10000) {
        out[0] = (char)(0xE0 | (codepoint >> 12));
        out[1] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        out[2] = (char)(0x80 | (codepoint & 0x3F));
        out[3] = '\0';
        return 3;
    }
    if(codepoint <= 0x10FFFF) {
        out[0] = (char)(0xF0 | (codepoint >> 18));
        out[1] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
        out[2] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        out[3] = (char)(0x80 | (codepoint & 0x3F));
        out[4] = '\0';
        return 4;
    }
    return 0;
}

static int
ui_text_delete_range(char *text, size_t text_size, int *cursor, int start, int end)
{
    int len;

    if(text == NULL || text_size == 0 || cursor == NULL)
        return 0;
    len = (int)strlen(text);
    start = ui_clampi(start, 0, len);
    end = ui_clampi(end, 0, len);
    if(end <= start)
        return 0;
    memmove(text + start, text + end, (size_t)(len - end + 1));
    *cursor = start;
    return 1;
}

static Rectangle
ui_world_rect_to_screen(Rectangle rect)
{
    return (Rectangle){
        g_ui_camera.offset.x + rect.x * g_ui_camera.zoom,
        g_ui_camera.offset.y + rect.y * g_ui_camera.zoom,
        rect.width * g_ui_camera.zoom,
        rect.height * g_ui_camera.zoom
    };
}

static void
ui_begin_world_clip(Rectangle rect)
{
    Rectangle screen = ui_world_rect_to_screen(rect);
    flint_clip_begin((int)screen.x, (int)screen.y,
                     (int)screen.width, (int)screen.height);
}

static void
ui_draw_text_centered_in_rect(const char *text, Rectangle rect, int font_size, Color color)
{
    const char *value = text != NULL ? text : "";
    int text_w = flint_text_measure(value, font_size);
    int x = (int)(rect.x + (rect.width - (float)text_w) * 0.5f);
    int y = flint_ui_text_y(value, (int)rect.y, (int)rect.height, font_size);
    int guard = 1;

    ui_begin_world_clip((Rectangle){rect.x, rect.y - guard,
                                    rect.width, rect.height + guard * 2});
    flint_text_draw(value, x, y, font_size, color);
    flint_clip_end();
}

static int
ui_text_next_smaller_size(int font_size)
{
    if(font_size > FLINT_TEXT_16)
        return FLINT_TEXT_16;
    if(font_size > FLINT_TEXT_12)
        return FLINT_TEXT_12;
    return FLINT_TEXT_8;
}

static int
ui_text_normalize_size(int font_size)
{
    if(font_size <= FLINT_TEXT_8)
        return FLINT_TEXT_8;
    if(font_size <= FLINT_TEXT_12)
        return FLINT_TEXT_12;
    if(font_size <= FLINT_TEXT_16)
        return FLINT_TEXT_16;
    return FLINT_TEXT_24;
}

void
flint_ui_draw_text_left_in_rect(const char *text, Rectangle rect, int font_size, Color color)
{
    const char *value = text != NULL ? text : "";
    int y = flint_ui_text_y(value, (int)rect.y, (int)rect.height, font_size);
    int guard = 1;

    ui_begin_world_clip((Rectangle){rect.x, rect.y - guard,
                                    rect.width, rect.height + guard * 2});
    flint_text_draw(value, (int)rect.x, y, font_size, color);
    flint_clip_end();
}

void
ui_draw_fitted_text_in_rect(const char *text, Rectangle rect,
                            int preferred_size, int min_size, Color color)
{
    const char *value = text != NULL ? text : "";
    int font_size = ui_text_normalize_size(preferred_size);
    int min_allowed = ui_text_normalize_size(min_size);

    if(font_size < min_allowed)
        font_size = min_allowed;
    while(font_size > min_allowed && flint_text_measure(value, font_size) > (int)rect.width)
        font_size = ui_text_next_smaller_size(font_size);
    ui_draw_text_centered_in_rect(value, rect, font_size, color);
}

static int
ui_text_insert_codepoint(char *text, size_t text_size, int *cursor, int codepoint,
                         int max_codepoints)
{
    char encoded[5];
    int encoded_len;
    int len;

    if(text == NULL || text_size == 0 || cursor == NULL || codepoint < 32)
        return 0;
    if(max_codepoints > 0 && ui_utf8_codepoint_count(text) >= max_codepoints)
        return 0;

    encoded_len = ui_utf8_encode(codepoint, encoded);
    if(encoded_len <= 0)
        return 0;
    len = (int)strlen(text);
    *cursor = ui_clampi(*cursor, 0, len);
    if((size_t)(len + encoded_len + 1) > text_size)
        return 0;

    memmove(text + *cursor + encoded_len, text + *cursor, (size_t)(len - *cursor + 1));
    memcpy(text + *cursor, encoded, (size_t)encoded_len);
    *cursor += encoded_len;
    return 1;
}

void
flint_ui_text_input_queue_codepoint(int codepoint)
{
    if(codepoint <= 0)
        return;
    if(g_ui_text_input_codepoint_count >= UI_TEXT_INPUT_QUEUE_MAX)
        return;
    g_ui_text_input_codepoints[g_ui_text_input_codepoint_count++] = codepoint;
}

void
flint_ui_text_input_queue_backspace(void)
{
    if(g_ui_text_input_backspace_count < UI_TEXT_INPUT_QUEUE_MAX)
        g_ui_text_input_backspace_count++;
}

void
flint_ui_text_input_queue_enter(void)
{
    if(g_ui_text_input_enter_count < UI_TEXT_INPUT_QUEUE_MAX)
        g_ui_text_input_enter_count++;
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
    active = active != 0;
    g_ui_focus_text_input_active = active;
    if(g_ui_platform_text_input_active != active) {
        g_ui_platform_text_input_active = active;
        if(g_ui_text_input_platform_callback != NULL)
            g_ui_text_input_platform_callback(active);
    }
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
    return FLINT_TEXT_16;
}

int
flint_ui_font_small(void)
{
    return FLINT_TEXT_12;
}

int
flint_ui_title_font(const char *title, int max_width)
{
    const char *value = title != NULL ? title : "";

    if(max_width <= 0 || flint_text_measure(value, FLINT_TEXT_24) <= max_width)
        return FLINT_TEXT_24;
    if(flint_text_measure(value, FLINT_TEXT_16) <= max_width)
        return FLINT_TEXT_16;
    return FLINT_TEXT_12;
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
    int clip_guard = 1;
    int text_x = x + padding_x;
    int text_y = flint_ui_text_y(value, y, h, font);
    Color border = focused ? style.focus_border : style.border;
    float radius = style.radius > 0.0f ? style.radius : 0.12f;

    if(focused)
        ui_focus_set_text_input_active(1);

    DrawRectangleRounded(bounds, radius, 8, style.background);
    DrawRectangleRoundedLines(bounds, radius, 8, border);

    ui_begin_world_clip((Rectangle){(float)(x + padding_x), (float)(y - clip_guard),
                                    (float)(w - padding_x * 2),
                                    (float)(h + clip_guard * 2)});
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
    flint_clip_end();
}

static int
ui_text_cursor_from_x(const char *text, int font, int text_x, int mouse_x)
{
    char prefix[1024];
    int len;

    if(text == NULL)
        return 0;

    len = (int)strlen(text);
    if(mouse_x <= text_x)
        return 0;

    for(int i = 1; i <= len; i++) {
        int copy_len = i;
        if(copy_len >= (int)sizeof(prefix))
            copy_len = (int)sizeof(prefix) - 1;
        memcpy(prefix, text, (size_t)copy_len);
        prefix[copy_len] = '\0';
        if(text_x + flint_text_measure(prefix, font) >= mouse_x)
            return i;
    }
    return len;
}

int
flint_ui_text_edit(FlintUITextEdit edit)
{
    int changed = 0;
    int len;
    int codepoint;

    if(edit.commit_pressed != NULL)
        *edit.commit_pressed = 0;
    if(edit.text == NULL || edit.text_size == 0 || edit.cursor_position == NULL)
        return 0;

    len = (int)strlen(edit.text);
    *edit.cursor_position = ui_clampi(*edit.cursor_position, 0, len);

    if(IsKeyPressed(KEY_LEFT)) {
        *edit.cursor_position = ui_utf8_prev_offset(edit.text, *edit.cursor_position);
        changed = 1;
    }
    if(IsKeyPressed(KEY_RIGHT)) {
        *edit.cursor_position = ui_utf8_next_offset(edit.text, *edit.cursor_position);
        changed = 1;
    }
    if(IsKeyPressed(KEY_HOME)) {
        *edit.cursor_position = 0;
        changed = 1;
    }
    if(IsKeyPressed(KEY_END)) {
        *edit.cursor_position = (int)strlen(edit.text);
        changed = 1;
    }

    {
        int repeat = g_ui_text_input_backspace_count + ui_backspace_repeat_count();
        for(int i = 0; i < repeat; i++) {
            int start = ui_utf8_prev_offset(edit.text, *edit.cursor_position);
            changed |= ui_text_delete_range(edit.text, edit.text_size,
                                            edit.cursor_position, start,
                                            *edit.cursor_position);
        }
        g_ui_text_input_backspace_count = 0;
    }

    if(IsKeyPressed(KEY_DELETE)) {
        int end = ui_utf8_next_offset(edit.text, *edit.cursor_position);
        changed |= ui_text_delete_range(edit.text, edit.text_size,
                                        edit.cursor_position,
                                        *edit.cursor_position, end);
    }

    if(IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) ||
       g_ui_text_input_enter_count > 0) {
        if(edit.commit_pressed != NULL)
            *edit.commit_pressed = 1;
        g_ui_text_input_enter_count = 0;
    }

    codepoint = GetCharPressed();
    while(codepoint > 0) {
        if((edit.filter == NULL || edit.filter(codepoint, edit.filter_user_data)) &&
           ui_text_insert_codepoint(edit.text, edit.text_size, edit.cursor_position,
                                    codepoint, edit.max_codepoints))
            changed = 1;
        codepoint = GetCharPressed();
    }

    for(int i = 0; i < g_ui_text_input_codepoint_count; i++) {
        codepoint = g_ui_text_input_codepoints[i];
        if((edit.filter == NULL || edit.filter(codepoint, edit.filter_user_data)) &&
           ui_text_insert_codepoint(edit.text, edit.text_size, edit.cursor_position,
                                    codepoint, edit.max_codepoints))
            changed = 1;
    }
    g_ui_text_input_codepoint_count = 0;

    len = (int)strlen(edit.text);
    *edit.cursor_position = ui_clampi(*edit.cursor_position, 0, len);
    return changed;
}

int
flint_ui_button(FlintUIButton button)
{
    Vector2 mouse_world = ui_mouse_world();
    int mouse_inside = CheckCollisionPointRec(mouse_world, button.bounds);
    int captured = ui_input_captures_click(mouse_world);
    int active = !button.disabled && !captured && mouse_inside;
    int hovered = active && ui_hover_effects_enabled();
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

    if(active) {
        ui_mark_clickable();
        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
            clicked = 1;
    }

    if(focused) {
        ui_focus_set_text_input_active(0);
        ui_focus_draw(button.bounds);
    }

    ui_draw_fitted_text_in_rect(button.label ? button.label : "", button.bounds,
                                font, FLINT_TEXT_16, text);
    return clicked || ui_focus_activate_pressed(button.focus_id);
}

int
flint_ui_icon_button(FlintUIIconButton button)
{
    Vector2 mouse_world = ui_mouse_world();
    int mouse_inside = CheckCollisionPointRec(mouse_world, button.bounds);
    int captured = ui_input_captures_click(mouse_world);
    int active = !button.disabled && !captured && mouse_inside;
    int hovered = active && ui_hover_effects_enabled();
    int focused = !button.disabled && button.focus_id > 0 && ui_focus_register(button.focus_id, button.bounds);
    int clicked = 0;
    int icon_padding = button.icon_padding > 0 ? button.icon_padding : flint_px(4);
    int draw_size = button.icon_size;
    Color background = button.background.a != 0 ? button.background : c_button;
    Color hover_background = button.hover_background.a != 0 ? button.hover_background : c_button_hover;
    Color icon_color = button.icon_color.a != 0 ? button.icon_color : c_text;
    Color border = button.border.a != 0 ? button.border : flint_darken(background, 35);
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

    if(active) {
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

int
flint_ui_text_field(FlintUITextField field)
{
    int changed = 0;
    int focused;
    int font;
    int padding_x;
    int commit_pressed = 0;
    Vector2 mouse_world;
    int mouse_inside;
    int captured;

    if(field.commit_pressed != NULL)
        *field.commit_pressed = 0;
    if(field.text == NULL || field.text_size == 0 ||
       field.cursor_position == NULL || field.focused == NULL)
        return 0;

    font = field.font > 0 ? field.font : flint_ui_font();
    padding_x = field.style.padding_x > 0 ? field.style.padding_x : flint_px(10);
    focused = *field.focused != 0;

    if(field.focus_id > 0 && ui_focus_register(field.focus_id, field.bounds)) {
        focused = 1;
        ui_focus_draw(field.bounds);
    }

    mouse_world = ui_mouse_world();
    mouse_inside = CheckCollisionPointRec(mouse_world, field.bounds);
    captured = ui_input_captures_click(mouse_world);

    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if(mouse_inside && !captured) {
            focused = 1;
            *field.cursor_position = ui_text_cursor_from_x(
                field.text, font, (int)field.bounds.x + padding_x,
                (int)mouse_world.x);
        } else if(focused) {
            focused = 0;
        }
    }

    *field.focused = focused;
    ui_focus_set_text_input_active(focused);

    if(focused) {
        changed = flint_ui_text_edit((FlintUITextEdit){
            .text = field.text,
            .text_size = field.text_size,
            .cursor_position = field.cursor_position,
            .max_codepoints = field.max_codepoints,
            .filter = field.filter,
            .filter_user_data = field.filter_user_data,
            .commit_pressed = field.commit_pressed != NULL
                                  ? field.commit_pressed
                                  : &commit_pressed
        });
    } else {
        int len = (int)strlen(field.text);
        *field.cursor_position = ui_clampi(*field.cursor_position, 0, len);
    }

    flint_ui_draw_text_input(field.bounds, field.text, *field.cursor_position,
                             focused, focused && (((int)(GetTime() * 2.0)) % 2 == 0),
                             font, field.style);
    return changed;
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
    Color color = paragraph.color.a != 0 ? paragraph.color : c_text;
    FlintTextLayout layout = flint_ui_paragraph_layout(paragraph);
    flint_text_layout_draw(&layout, x, y, font, color);
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

int
ui_is_desktop_mode(void)
{
    return ui_view_width >= flint_px(500);
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
    ui_update_pointer_gesture();
    g_ui_input_blocked = 0;
    g_ui_input_clip_stack_count = 0;
    flint_clip_reset();
}

void
flint_ui_set_text_input_platform_callback(FlintUITextInputPlatformCallback callback)
{
    g_ui_text_input_platform_callback = callback;
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
        int show_hover = ui_hover_effects_enabled();
        Color fill = show_hover ? c_button_hover : c_button;
        DrawRectangle(x, y, w, h, fill);
        ui_draw_bevel(x, y, w, h, flint_lighten(fill, 40), flint_darken(fill, 40));
        *hover = show_hover;
        ui_mark_clickable();
        if(show_hover && mb) {
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
        int show_hover = ui_hover_effects_enabled();
        Color fill = show_hover ? c_button_hover : c_button;
        DrawRectangle(x, y, w, h, fill);
        ui_draw_bevel(x, y, w, h, flint_lighten(fill, 40), flint_darken(fill, 20));
        *hover = show_hover;
        ui_mark_clickable();
        if(show_hover && mb) {
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
    int local_hover = 0;

    int mb = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    int released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    int font = FLINT_TEXT_16;
    int w = (int)flint_text_measure(label, font) + flint_px(24);
    int h = flint_text_line_height(font) + flint_px(12);

    x = x - w / 2;
    if(!hover)
        hover = &local_hover;

    int pressed = 0;

    if(mx > x && mx < x + w && my > y && my < y + h && !ui_input_captures_click(mouse_world)) {
        int show_hover = ui_hover_effects_enabled();
        Color fill = show_hover ? c_button_hover : c_button;
        DrawRectangle(x, y, w, h, fill);
        ui_draw_bevel(x, y, w, h, flint_lighten(fill, 40), flint_darken(fill, 40));
        *hover = show_hover;
        ui_mark_clickable();
        if(show_hover && mb) {
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

    ui_draw_fitted_text_in_rect(label, (Rectangle){(float)x, (float)y, (float)w, (float)h},
                                font, FLINT_TEXT_12, c_text);

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
    int active = mouse_inside && !disabled && !captured;
    int hovered = active && ui_hover_effects_enabled();

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

    if(active) {
        Color draw_bg = hovered ? hover_bg : bg;
        DrawRectangleRec(bounds, draw_bg);
        ui_draw_bevel(x, y, w, h, flint_lighten(draw_bg, 40), flint_darken(draw_bg, 40));
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
        int label_pad = flint_px(4);
        Rectangle tab_rect = {(float)tab_x, bar.bounds.y, (float)draw_w, bar.bounds.height};
        Rectangle label_rect = {(float)(tab_x + label_pad), bar.bounds.y,
                                (float)(draw_w - label_pad * 2), bar.bounds.height};
        int input_captured = ui_input_captures_click(mouse_world);
        int is_active = CheckCollisionPointRec(mouse_world, tab_rect) && !input_captured;
        int is_hovered = is_active && ui_hover_effects_enabled();
        int is_selected = i == bar.selected_index;
        int is_disabled = bar.tabs[i].disabled;
        Color accent = bar.tabs[i].accent.a != 0 ? bar.tabs[i].accent : c_button_hover;
        Color text_color = c_text;
        const char *label = bar.tabs[i].label ? bar.tabs[i].label : "";
        Texture2D icon = bar.tabs[i].icon;
        int icon_size = bar.tabs[i].icon_size > 0 ? bar.tabs[i].icon_size : flint_px(20);

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

        if(is_active) {
            if(is_disabled)
                ui_mark_disabled();
            else if(!is_selected)
                ui_mark_clickable();

            if(released)
                clicked_tab = i;
        }

        if(label_rect.width < 1)
            label_rect.width = 1;
        if(icon.id != 0) {
            Rectangle src = {0, 0, (float)icon.width, (float)icon.height};
            Rectangle dst = {
                tab_x + (draw_w - icon_size) / 2.0f,
                bar_y + (tab_h - icon_size) / 2.0f,
                (float)icon_size,
                (float)icon_size
            };
            Color icon_color = is_disabled ? flint_darken(c_icon, 40) : c_icon;
            DrawTexturePro(icon, src, dst, (Vector2){0}, 0, icon_color);
        } else {
            ui_draw_fitted_text_in_rect(label, label_rect, font, FLINT_TEXT_8, text_color);
        }
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
        hover = ui_hover_effects_enabled();
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

    if(mx > btn_x && mx < btn_x + btn_w && my > btn_y && my < btn_y + btn_h &&
       !ui_input_captures_click(mouse_world) && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
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

    if(g_ui_slider_active_id == id &&
       !IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
       !IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
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

    if(g_ui_slider_active_id == id && g_ui_pointer_owner == UI_POINTER_OWNER_NONE &&
       g_ui_pointer_dragging) {
        if(ui_pointer_drag_is_horizontal())
            g_ui_pointer_owner = UI_POINTER_OWNER_HORIZONTAL_SLIDER;
        else
            g_ui_slider_active_id = 0;
    }

    if(g_ui_slider_active_id == id &&
       ((IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
         g_ui_pointer_owner == UI_POINTER_OWNER_HORIZONTAL_SLIDER) ||
        IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) &&
       !ui_input_captures_click_internal(mouse_world, 0)) {
        int old_value = *value;
        float t = (float)(mx - x) / (float)w;
        if(t < 0.0f)
            t = 0.0f;
        if(t > 1.0f)
            t = 1.0f;
        *value = min + (int)(t * (float)(max - min) + 0.5f);
        *value = ui_clampi(*value, min, max);
        changed = (*value != old_value);
        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
            g_ui_slider_active_id = 0;
    } else if(g_ui_slider_active_id == id && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        g_ui_slider_active_id = 0;
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

    if(g_ui_slider_active_id == id &&
       !IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
       !IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        g_ui_slider_active_id = 0;

    /* Draw track */
    DrawRectangle(track_x, y, track_w, h, flint_darken(c_bg, 28));
    ui_draw_bevel(track_x, y, track_w, h, flint_darken(c_bg, 55), flint_lighten(c_bg, 35));

    /* Check for interaction */
    if(CheckCollisionPointRec(mouse_world, hit) && !ui_input_captures_click(mouse_world)) {
        ui_mark_clickable();
        if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            g_ui_slider_active_id = id;
            g_ui_pointer_owner = UI_POINTER_OWNER_VERTICAL_SLIDER;
        }
    }

    /* Handle drag */
    if(g_ui_slider_active_id == id &&
       (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) &&
       !ui_input_captures_click_internal(mouse_world, 0)) {
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
        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
            g_ui_slider_active_id = 0;
    } else if(g_ui_slider_active_id == id && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        g_ui_slider_active_id = 0;
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

    if(g_ui_slider_active_id == id &&
       !IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
       !IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
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
        if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            g_ui_slider_active_id = id;
            g_ui_pointer_owner = UI_POINTER_OWNER_VERTICAL_SLIDER;
        }
    }

    /* Handle drag */
    if(g_ui_slider_active_id == id &&
       (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) &&
       !ui_input_captures_click_internal(mouse_world, 0)) {
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
        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
            g_ui_slider_active_id = 0;
    } else if(g_ui_slider_active_id == id && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        g_ui_slider_active_id = 0;
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
        ui_mark_clickable();
    }

    int pressed = CheckCollisionPointRec(mouse_world, bounds) &&
                  !ui_input_captures_click(mouse_world) &&
                  IsMouseButtonReleased(MOUSE_BUTTON_LEFT);

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
ui_draw_checkbox_toggle_disabled(int x, int y, const char *label,
                                int *value, int disabled)
{
    int font = flint_ui_font();
    int box_size = flint_px(22);
    int label_gap = flint_px(10);
    int label_w = flint_text_measure(label, font);
    int label_h = flint_text_line_height(font);
    int row_h = box_size > label_h ? box_size : label_h;
    Rectangle bounds = {x, y, box_size + label_gap + label_w, row_h};
    Vector2 mouse_world = ui_mouse_world();
    Color box_color = disabled ? flint_darken(c_button, 18) : c_button;
    Color mark_color = disabled ? flint_darken(c_text, 35) : c_text;
    Color label_color = disabled ? flint_darken(c_text, 35) : c_text;

    if(CheckCollisionPointRec(mouse_world, bounds) && !ui_input_captures_click(mouse_world)) {
        if(disabled)
            ui_mark_disabled();
        else {
            ui_mark_clickable();
        }
    }

    int pressed = CheckCollisionPointRec(mouse_world, bounds) && !disabled &&
                  !ui_input_captures_click(mouse_world) &&
                  IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    if(pressed)
        *value = !(*value);

    DrawRectangle(x, y + (row_h - box_size) / 2, box_size, box_size, box_color);
    ui_draw_bevel(x, y + (row_h - box_size) / 2, box_size, box_size,
                  flint_darken(c_bg, 30), flint_lighten(c_bg, 20));

    if(*value) {
        int padding = flint_px(4);
        int box_y = y + (row_h - box_size) / 2;
        DrawLine(x + padding, box_y + padding, x + box_size / 2,
                 box_y + box_size - padding, mark_color);
        DrawLine(x + box_size / 2, box_y + box_size - padding,
                 x + box_size - padding, box_y + padding, mark_color);
    }

    flint_text_draw(label, x + box_size + label_gap,
                    flint_ui_text_y(label, y, row_h, font),
                    font, label_color);

    return pressed;
}

int
ui_draw_checkbox_toggle(int x, int y, const char *label, int *value)
{
    return ui_draw_checkbox_toggle_disabled(x, y, label, value, 0);
}

int
ui_draw_info_button(int center_x, int center_y, int diameter)
{
    Vector2 mouse_world = ui_mouse_world();
    int min_touch = flint_px(32);
    int radius;
    int active = 0;
    int hover = 0;
    Rectangle hit;
    Color fill;
    Color stroke;
    Color text;
    int font;

    if(diameter <= 0)
        diameter = flint_px(18);
    radius = diameter / 2;
    hit = ui_centered_min_hit_rect(center_x - radius, center_y - radius,
                                  diameter, diameter, min_touch, min_touch);

    active = CheckCollisionPointRec(mouse_world, hit) && !ui_input_captures_click(mouse_world);
    if(active) {
        hover = ui_hover_effects_enabled();
        ui_mark_clickable();
    }

    fill = hover ? c_button_hover : flint_darken(c_bg, 8);
    stroke = c_text;
    text = c_text;
    DrawCircle(center_x, center_y, radius, fill);
    DrawCircleLines(center_x, center_y, radius, stroke);
    font = flint_ui_font_small();
    flint_ui_draw_text_centered("i", center_x, center_y, font, text);

    return active && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}
