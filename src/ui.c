#include "ui.h"
#include "android_insets.h"
#include "app.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Global UI state */
static float dpi_scale = 1.0f;
static int ui_view_width = 320;
static int ui_view_height = 560;
static Color c_text, c_bg, c_circle, c_button, c_button_hover, c_icon;

void
ui_init(int width, int height, float dpi)
{
    ui_view_width = width;
    ui_view_height = height;
    dpi_scale = dpi;
}

void
ui_set_colors(Color text, Color bg, Color circle, Color button, Color button_hover, Color icon)
{
    c_text = text;
    c_bg = bg;
    c_circle = circle;
    c_button = button;
    c_button_hover = button_hover;
    c_icon = icon;
}

/* ================================================================
 * DPI SCALING
 * ================================================================ */

int
ui_px(int px)
{
    return (int)(px * dpi_scale + 0.5f);
}

int
ui_clamp_px(int px, int min_px, int max_px)
{
    int value = (int)(px * dpi_scale + 0.5f);
    int min_value = (int)(min_px * dpi_scale + 0.5f);
    int max_value = (int)(max_px * dpi_scale + 0.5f);

    if(value < min_value)
        value = min_value;
    if(value > max_value)
        value = max_value;
    return value;
}

/* ================================================================
 * COLOR HELPERS
 * ================================================================ */

Color
ui_lighten(Color c, int amount)
{
    return (Color){
        (unsigned char)(c.r + amount > 255 ? 255 : c.r + amount),
        (unsigned char)(c.g + amount > 255 ? 255 : c.g + amount),
        (unsigned char)(c.b + amount > 255 ? 255 : c.b + amount),
        c.a
    };
}

Color
ui_darken(Color c, int amount)
{
    return (Color){
        (unsigned char)(c.r < amount ? 0 : c.r - amount),
        (unsigned char)(c.g < amount ? 0 : c.g - amount),
        (unsigned char)(c.b < amount ? 0 : c.b - amount),
        c.a
    };
}

/* ================================================================
 * LAYOUT HELPERS
 * ================================================================ */

void
ui_centered_column(int max_w, int side_pad, int *x, int *w)
{
    max_w = (int)(max_w * dpi_scale + 0.5f);
    side_pad = (int)(side_pad * dpi_scale + 0.5f);

    int available_w = ui_view_width - side_pad * 2;

    if(available_w < 0)
        available_w = 0;
    if(max_w > available_w)
        max_w = available_w;
    if(max_w < 0)
        max_w = 0;

    if(x != NULL)
        *x = (ui_view_width - max_w) / 2;
    if(w != NULL)
        *w = max_w;
}

void
ui_draw_bevel(int x, int y, int w, int h, Color light, Color dark)
{
    int border = (int)(dpi_scale + 0.5f);
    if(border < 1) border = 1;

    DrawRectangle(x, y, w, border, light);
    DrawRectangle(x, y, border, h, light);
    DrawRectangle(x + w - border, y, border, h, dark);
    DrawRectangle(x, y + h - border, w, border, dark);
}

void
ui_draw_text_lines(const char **lines, int count, int x, int *y, int font, int line_h)
{
    for(int i = 0; i < count; i++) {
        DrawText(lines[i], x, *y, font, c_text);
        *y += line_h;
    }
}

/* ================================================================
 * ICON BUTTONS
 * ================================================================ */

int
ui_icon_btn_size(UIIconSize size)
{
    switch(size) {
        case UI_ICON_SIZE_TINY:    return ui_clamp_px(18, 16, 40);
        case UI_ICON_SIZE_SMALL:   return ui_clamp_px(ICON_SIZE_SMALL, ICON_SIZE_SMALL_MIN, ICON_SIZE_SMALL_MAX);
        case UI_ICON_SIZE_MEDIUM:  return ui_clamp_px(ICON_SIZE_MEDIUM, ICON_SIZE_MEDIUM_MIN, ICON_SIZE_MEDIUM_MAX);
        case UI_ICON_SIZE_LARGE:   return ui_clamp_px(ICON_SIZE_LARGE, ICON_SIZE_LARGE_MIN, ICON_SIZE_LARGE_MAX);
        default: return ui_clamp_px(ICON_SIZE_SMALL, ICON_SIZE_SMALL_MIN, ICON_SIZE_SMALL_MAX);
    }
}

int
ui_icon_btn_padding(UIIconSize size)
{
    switch(size) {
        case UI_ICON_SIZE_TINY:    return ui_px(8);
        case UI_ICON_SIZE_SMALL:   return ui_px(10);
        case UI_ICON_SIZE_MEDIUM:  return ui_px(12);
        case UI_ICON_SIZE_LARGE:   return ui_px(14);
        default: return ui_px(10);
    }
}

int
ui_draw_icon_btn(InbeApp *app, int x, int y, UIIconSize size, Texture2D icon, int *hover)
{
    int btn_size = ui_icon_btn_size(size);
    int padding = ui_icon_btn_padding(size);
    int w = btn_size + padding * 2;
    int h = btn_size + padding * 2;

    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
    int mx = (int)mouse_world.x;
    int my = (int)mouse_world.y;
    int mb = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    int released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    int pressed = 0;

    if(mx > x && mx < x + w && my > y && my < y + h) {
        DrawRectangle(x, y, w, h, c_button_hover);
        ui_draw_bevel(x, y, w, h, ui_darken(c_button_hover, 40), ui_lighten(c_button_hover, 40));
        *hover = 1;
        app->cursor_clickable = 1;
        if(mb) {
            ui_draw_bevel(x, y, w, h, ui_lighten(c_button_hover, 40), ui_darken(c_button_hover, 40));
        }
        if(released && !ui_dropdown_captures_click(mouse_world)) {
            pressed = 1;
        }
    } else {
        DrawRectangle(x, y, w, h, c_button);
        ui_draw_bevel(x, y, w, h, ui_lighten(c_button, 40), ui_darken(c_button, 40));
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
ui_draw_icon_btn_padded(InbeApp *app, int x, int y, int size, Texture2D icon, int *hover)
{
    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
    int mx = (int)mouse_world.x;
    int my = (int)mouse_world.y;
    int mb = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    int released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    int padding = ui_px(10);
    int w = size + padding * 2;
    int h = size + padding * 2;
    int pressed = 0;

    if(mx > x && mx < x + w && my > y && my < y + h) {
        DrawRectangle(x, y, w, h, c_button_hover);
        ui_draw_bevel(x, y, w, h, ui_darken(c_button_hover, 40), ui_lighten(c_button_hover, 40));
        *hover = 1;
        app->cursor_clickable = 1;
        if(mb) {
            ui_draw_bevel(x, y, w, h, ui_lighten(c_button_hover, 40), ui_darken(c_button_hover, 40));
        }
        if(released && !ui_dropdown_captures_click(mouse_world)) {
            pressed = 1;
        }
    } else {
        DrawRectangle(x, y, w, h, c_button);
        ui_draw_bevel(x, y, w, h, ui_lighten(c_button, 40), ui_darken(c_button, 20));
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
ui_draw_text_btn(InbeApp *app, int x, int y, const char *label, int *hover)
{
    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
    int mx = (int)mouse_world.x;
    int my = (int)mouse_world.y;

    int mb = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    int released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    int font = ui_clamp_px(20, 16, 22);
    int w = (int)MeasureText(label, font) + ui_px(20);
    int h = ui_clamp_px(30, 26, 34);

    x = x - w / 2;

    int pressed = 0;

    if(mx > x && mx < x + w && my > y && my < y + h) {
        DrawRectangle(x, y, w, h, c_button_hover);
        ui_draw_bevel(x, y, w, h, ui_darken(c_button_hover, 40), ui_lighten(c_button_hover, 40));
        *hover = 1;
        app->cursor_clickable = 1;
        if(mb) {
            ui_draw_bevel(x, y, w, h, ui_lighten(c_button_hover, 40), ui_darken(c_button_hover, 40));
        }
        if(released && !ui_dropdown_captures_click(mouse_world)) {
            pressed = 1;
        }
    } else {
        DrawRectangle(x, y, w, h, c_button);
        ui_draw_bevel(x, y, w, h, ui_lighten(c_button, 40), ui_darken(c_button, 40));
        *hover = 0;
    }

    DrawText(label, x + ui_px(10), y + ui_px(5), font, c_text);

    return pressed;
}

void
ui_draw_icon_link(InbeApp *app, int x, int y, int icon_size, Texture2D icon, const char *url)
{
    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
    int mx = (int)mouse_world.x;
    int my = (int)mouse_world.y;
    int hover = 0;
    int padding = ui_px(4);
    int btn_w = icon_size + padding * 2;
    int btn_h = icon_size + padding * 2;
    int btn_x = x - padding;
    int btn_y = y - padding;

    if(mx > btn_x && mx < btn_x + btn_w && my > btn_y && my < btn_y + btn_h) {
        hover = 1;
        app->cursor_clickable = 1;
    }

    if(hover) {
        DrawRectangle(btn_x, btn_y, btn_w, btn_h, c_button_hover);
        ui_draw_bevel(btn_x, btn_y, btn_w, btn_h, ui_darken(c_button_hover, 40), ui_lighten(c_button_hover, 40));
    } else {
        DrawRectangle(btn_x, btn_y, btn_w, btn_h, c_button);
        ui_draw_bevel(btn_x, btn_y, btn_w, btn_h, ui_lighten(c_button, 40), ui_darken(c_button, 40));
    }

    if(icon.id != 0) {
        Rectangle src = {0, 0, icon.width, icon.height};
        Rectangle dst = {x, y, (float)icon_size, (float)icon_size};
        DrawTexturePro(icon, src, dst, (Vector2){0}, 0, c_icon);
    }

    if(hover && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && !ui_dropdown_captures_click(mouse_world)) {
        OpenURL(url);
    }
}

/* ================================================================
 * CONTROLS
 * ================================================================ */

int
ui_draw_slider(InbeApp *app, int id, int x, int y, int w, const char *label,
               int min, int max, int *value, const char *suffix)
{
    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
    int mx = (int)mouse_world.x;
    int label_font = ui_clamp_px(14, 12, 16);
    int value_font = ui_clamp_px(14, 12, 16);
    int track_y = y + ui_px(28);
    int track_h = ui_px(8);
    int knob_w = ui_px(12);
    int knob_h = ui_px(22);
    int knob_y = track_y - (knob_h - track_h) / 2;
    int hit_padding = ui_px(16);
    int changed = 0;
    char value_text[32];
    Rectangle hit = {(float)(x - hit_padding), (float)(knob_y - hit_padding), (float)(w + hit_padding * 2), (float)(knob_h + hit_padding * 2)};

    snprintf(value_text, sizeof(value_text), "%d%s", *value, suffix != NULL ? suffix : "");
    DrawText(label, x, y, label_font, c_text);
    DrawText(value_text, x + w - MeasureText(value_text, value_font), y, value_font, c_text);

    DrawRectangle(x, track_y, w, track_h, ui_darken(c_bg, 28));
    ui_draw_bevel(x, track_y, w, track_h, ui_darken(c_bg, 55), ui_lighten(c_bg, 35));

    if(CheckCollisionPointRec(mouse_world, hit)) {
        app->cursor_clickable = 1;
        if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !ui_dropdown_captures_click(mouse_world))
            app->settings_drag_slider = id;
    }

    if(app->settings_drag_slider == id && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        int old_value = *value;
        float t = (float)(mx - x) / (float)w;
        if(t < 0.0f)
            t = 0.0f;
        if(t > 1.0f)
            t = 1.0f;
        *value = min + (int)(t * (float)(max - min) + 0.5f);
        *value = clampi(*value, min, max);
        changed = (*value != old_value);
    }

    float t = (float)(*value - min) / (float)(max - min);
    int knob_x = x + (int)(t * (float)w) - knob_w / 2;
    DrawRectangle(knob_x, knob_y, knob_w, knob_h, c_button);
    ui_draw_bevel(knob_x, knob_y, knob_w, knob_h, ui_lighten(c_button, 40), ui_darken(c_button, 40));

    return changed;
}

int
ui_draw_toggle_switch(InbeApp *app, int x, int y, int w, int h, int *value)
{
    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
    int hover = 0;
    Rectangle bounds = {x, y, w, h};

    if(CheckCollisionPointRec(mouse_world, bounds)) {
        hover = 1;
        app->cursor_clickable = 1;
    }

    int pressed = hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !ui_dropdown_captures_click(mouse_world);

    if(pressed)
        *value = !(*value);

    Color bg = ui_darken(c_bg, 8);
    DrawRectangle(x, y, w, h, bg);

    int track_h = h - 6;
    int track_y = y + 3;
    DrawRectangleRounded((Rectangle){x + 3, track_y, w - 6, track_h}, 0.5f, 8, ui_darken(c_bg, 20));

    int active_w = (w - 6) / 2;
    int active_x = *value ? x + w - active_w - 3 : x + 3;
    DrawRectangleRounded((Rectangle){active_x, track_y, active_w, track_h}, 0.5f, 8, c_circle);

    int font = ui_clamp_px(12, 10, 14);
    const char *light_label = "Light";
    const char *dark_label = "Dark";
    int light_w = MeasureText(light_label, font);
    int dark_w = MeasureText(dark_label, font);

    Color label_color = c_text;
    /* Center text in each half of the toggle */
    int light_x = x + w / 4 - light_w / 2;
    int dark_x = x + w * 3 / 4 - dark_w / 2;
    int text_y = y + h / 2 - font / 2;
    DrawText(light_label, light_x, text_y, font, label_color);
    DrawText(dark_label, dark_x, text_y, font, label_color);

    return pressed;
}

int
ui_draw_checkbox_toggle(InbeApp *app, int x, int y, const char *label, int *value)
{
    int font = ui_clamp_px(14, 12, 16);
    int box_size = ui_px(22);
    int hover = 0;
    Rectangle bounds = {x, y, box_size, box_size};
    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);

    if(CheckCollisionPointRec(mouse_world, bounds)) {
        hover = 1;
        app->cursor_clickable = 1;
    }

    int pressed = hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !ui_dropdown_captures_click(mouse_world);
    if(pressed)
        *value = !(*value);

    DrawRectangle(x, y, box_size, box_size, c_button);
    ui_draw_bevel(x, y, box_size, box_size, ui_darken(c_bg, 30), ui_lighten(c_bg, 20));

    if(*value) {
        int padding = ui_px(4);
        DrawLine(x + padding, y + padding, x + box_size / 2, y + box_size - padding, c_text);
        DrawLine(x + box_size / 2, y + box_size - padding, x + box_size - padding, y + padding, c_text);
    }

    DrawText(label, x + box_size + ui_px(10), y + (box_size - font) / 2, font, c_text);

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
} UIDropdownState;

#define MAX_DROPDOWNS 8
static UIDropdownState dropdown_states[MAX_DROPDOWNS];
static int dropdown_state_count = 0;

/* Check if any dropdown is currently open and the given point is within its menu bounds.
 * Other UI elements should call this to avoid handling clicks that should go to dropdowns. */
int
ui_dropdown_captures_click(Vector2 point)
{
    for(int i = 0; i < dropdown_state_count; i++) {
        UIDropdownState *state = &dropdown_states[i];
        if(state->open && state->option_count > 0) {
            int option_h = state->h;
            int menu_gap = ui_px(4);
            int dropdown_y = state->y + state->h + menu_gap;
            int dropdown_h = option_h * state->option_count + ui_px(8);
            int max_visible_h = ui_view_height - dropdown_y - ui_px(16);

            if(dropdown_h > max_visible_h) {
                int visible_options = (max_visible_h - ui_px(8)) / option_h;
                if(visible_options < 1)
                    visible_options = 1;
                dropdown_h = visible_options * option_h + ui_px(8);
            }

            Rectangle menu_bounds = {state->x, dropdown_y, state->w, dropdown_h};
            if(CheckCollisionPointRec(point, menu_bounds))
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
        return &dropdown_states[dropdown_state_count++];
    }

    /* Fallback - use first slot */
    dropdown_states[0].id = id;
    return &dropdown_states[0];
}

int
ui_draw_dropdown_button(InbeApp *app, int id, int x, int y, int w, int h,
                        const char **options, int option_count, int *selected_index)
{
    UIDropdownState *state = get_or_create_dropdown_state(id);
    int font = ui_clamp_px(14, 12, 16);
    int arrow_pad = ui_px(24);
    int arrow_size = ui_px(6);
    int changed = 0;
    Rectangle btn_bounds = {x, y, w, h};
    Vector2 mouse = GetScreenToWorld2D(GetMousePosition(), app->camera);
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
        app->cursor_clickable = 1;

    /* Handle click on button */
    if(hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        state->open = !state->open;
        if(state->open)
            state->just_opened = 1;
    }

    /* Mouse wheel to cycle through options */
    float wheel = GetMouseWheelMove();
    if(hover && wheel != 0 && !state->open) {
        if(wheel > 0)
            *selected_index = (*selected_index + 1) % option_count;
        else
            *selected_index = (*selected_index - 1 + option_count) % option_count;
        state->open = 0;
        changed = 1;
    }

    /* Draw button background */
    DrawRectangleRounded(btn_bounds, 0.3f, 8, ui_darken(c_bg, 8));

    /* Draw current selection text */
    const char *current_name = options[*selected_index];
    DrawText(current_name, x + ui_px(12), y + h / 2 - font / 2 - 1, font, c_text);

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

void
ui_draw_dropdown_menu(InbeApp *app, int id)
{
    UIDropdownState *state = get_or_create_dropdown_state(id);

    if(!state->open || state->options == NULL || state->selected_index == NULL)
        return;

    int font = ui_clamp_px(14, 12, 16);
    int x = state->x;
    int y = state->y;
    int w = state->w;
    int h = state->h;
    int option_count = state->option_count;
    int *selected_index = state->selected_index;
    const char **options = state->options;

    int option_h = h;
    int menu_gap = ui_px(4);
    int dropdown_y = y + h + menu_gap;
    int dropdown_h = option_h * option_count + ui_px(8);
    int max_visible_h = ui_view_height - dropdown_y - ui_px(16);
    int need_scroll = dropdown_h > max_visible_h;
    int visible_options = option_count;

    if(need_scroll) {
        visible_options = (max_visible_h - ui_px(8)) / option_h;
        if(visible_options < 1)
            visible_options = 1;
        dropdown_h = visible_options * option_h + ui_px(8);
    }

    Rectangle menu_bounds = {x, dropdown_y, w, dropdown_h};
    Rectangle btn_bounds = {x, y, w, h};
    Vector2 mouse = GetScreenToWorld2D(GetMousePosition(), app->camera);

    /* Click outside closes dropdown */
    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if(!state->just_opened &&
           !CheckCollisionPointRec(mouse, btn_bounds) &&
           !CheckCollisionPointRec(mouse, menu_bounds)) {
            state->open = 0;
        }
    }

    if(state->just_opened && !IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        state->just_opened = 0;

    /* Draw dropdown background */
    DrawRectangle(x, dropdown_y, w, dropdown_h, c_button);
    ui_draw_bevel(x, dropdown_y, w, dropdown_h, ui_darken(c_bg, 30), ui_lighten(c_bg, 20));

    /* Clip to menu area */
    BeginScissorMode(x, dropdown_y, w, dropdown_h);

    /* Draw options */
    for(int i = 0; i < option_count; i++) {
        int option_y = dropdown_y + ui_px(4) + (i - state->scroll_offset) * option_h;
        Rectangle option_bounds = {x, option_y, w, option_h};

        /* Skip if outside visible area */
        if(option_y + option_h < dropdown_y || option_y > dropdown_y + dropdown_h)
            continue;

        int option_hover = CheckCollisionPointRec(mouse, option_bounds);

        if(option_hover) {
            DrawRectangle(x, option_y, w, option_h, c_button_hover);
            app->cursor_clickable = 1;

            if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && !state->just_opened) {
                *selected_index = i;
                state->open = 0;
                state->just_opened = 0;
            }
        }

        DrawText(options[i], x + ui_px(12), option_y + option_h / 2 - font / 2 - 1, font, c_text);
    }

    EndScissorMode();

    /* Redraw arrow on top of everything */
    int arrow_pad = ui_px(24);
    int arrow_size = ui_px(6);
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
}

int
ui_nav_button_width(const char *label, int icon_size, int show_label, int font)
{
    int padding = ui_px(6);
    int width = icon_size + padding * 2;

    if(show_label && label != NULL && label[0] != '\0')
        width += ui_px(10) + MeasureText(label, font);
    return width;
}

int
ui_draw_nav_button(InbeApp *app, int x, int y, int icon_size, Texture2D icon,
                   const char *label, int show_label, int *hover)
{
    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
    int mx = (int)mouse_world.x;
    int my = (int)mouse_world.y;
    int mb = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    int released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    int font = ui_clamp_px(14, 12, 16);
    int padding = ui_px(6);
    int w = ui_nav_button_width(label, icon_size, show_label, font);
    int h = icon_size + padding * 2;
    int pressed = 0;

    if(mx > x && mx < x + w && my > y && my < y + h) {
        DrawRectangle(x, y, w, h, c_button_hover);
        ui_draw_bevel(x, y, w, h, ui_darken(c_button_hover, 40), ui_lighten(c_button_hover, 40));
        *hover = 1;
        app->cursor_clickable = 1;
        if(mb)
            ui_draw_bevel(x, y, w, h, ui_lighten(c_button_hover, 40), ui_darken(c_button_hover, 40));
        if(released)
            pressed = 1;
    } else {
        DrawRectangle(x, y, w, h, c_button);
        ui_draw_bevel(x, y, w, h, ui_lighten(c_button, 40), ui_darken(c_button, 40));
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
        int text_x = x + icon_size + padding * 2 + ui_px(10);
        int text_y = y + (h - font) / 2;
        DrawText(label, text_x, text_y, font, c_text);
    }

    return pressed;
}

int
ui_draw_nav_button_expand(InbeApp *app, int x, int y, int icon_size, int w, Texture2D icon,
                           const char *label, int show_label, int *hover)
{
    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
    int mx = (int)mouse_world.x;
    int my = (int)mouse_world.y;
    int mb = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    int released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    int font = ui_clamp_px(14, 12, 16);
    int padding = ui_px(6);
    int h = icon_size + padding * 2;
    int pressed = 0;

    if(mx > x && mx < x + w && my > y && my < y + h) {
        DrawRectangle(x, y, w, h, c_button_hover);
        ui_draw_bevel(x, y, w, h, ui_darken(c_button_hover, 40), ui_lighten(c_button_hover, 40));
        *hover = 1;
        app->cursor_clickable = 1;
        if(mb)
            ui_draw_bevel(x, y, w, h, ui_lighten(c_button_hover, 40), ui_darken(c_button_hover, 40));
        if(released)
            pressed = 1;
    } else {
        DrawRectangle(x, y, w, h, c_button);
        ui_draw_bevel(x, y, w, h, ui_lighten(c_button, 40), ui_darken(c_button, 40));
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
        int text_x = x + icon_size + padding * 2 + ui_px(10);
        int text_y = y + (h - font) / 2;
        DrawText(label, text_x, text_y, font, c_text);
    }

    return pressed;
}

/* ================================================================
 * TAB BAR
 * ================================================================ */

void
ui_draw_tab_bar(UITab *tabs, int count, InbeApp *app)
{
    int bar_h = ui_clamp_px(58, 54, 66);
    AndroidInsets insets = {0};
    android_insets_get(&insets);
    int bar_y = ui_view_height - bar_h;
    int button_size = ui_clamp_px(ICON_SIZE_LARGE, ICON_SIZE_LARGE_MIN, ICON_SIZE_LARGE_MAX);
    int button_h = button_size + ui_px(12);
    int font = ui_clamp_px(14, 12, 16);
    int side_margin = ui_px(16);
    int group_gap = ui_px(10);
    int available_w = ui_view_width - side_margin * 2;

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

    /* Only show labels if all buttons with labels fit */
    int show_labels = group_w_label <= available_w;
    int base_group_w = show_labels ? group_w_label : group_w_no_label;

    /* Calculate extra space and distribute evenly among buttons */
    int extra_w = available_w - base_group_w - group_gap * (count - 1);
    int extra_per_button = count > 0 ? extra_w / count : 0;
    int remainder = count > 0 ? extra_w % count : 0;

    /* Calculate total width with extra space included */
    int total_w = base_group_w + extra_per_button * count + group_gap * (count - 1);

    /* Center the tab group - remainder adds extra left margin for true centering */
    int group_x = side_margin + (available_w - total_w) / 2 + remainder / 2;
    int button_y = bar_y + (bar_h - button_h) / 2;
    int tab_hover = 0;

    DrawRectangle(0, bar_y, ui_view_width, bar_h, ui_darken(c_bg, 10));
    DrawLine(0, bar_y, ui_view_width, bar_y, ui_darken(c_bg, 42));

    int x = group_x;
    for(int i = 0; i < count; i++) {
        int base_w = ui_nav_button_width(tabs[i].label, button_size, show_labels, font);
        int w = base_w + extra_per_button;

        if(tabs[i].icon.id != 0) {
            if(ui_draw_nav_button_expand(app, x, button_y, button_size, w, tabs[i].icon,
                                        tabs[i].label, show_labels, &tab_hover)) {
                if(tabs[i].on_click)
                    tabs[i].on_click(tabs[i].user_data);
            }
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
    DrawRectangle(x, y, w, h, ui_darken(c_bg, 12));
    ui_draw_bevel(x, y, w, h, ui_darken(c_bg, 45), ui_lighten(c_bg, 35));
    int font = ui_clamp_px(14, 12, 16);
    int tw = MeasureText(label, font);
    DrawText(label, x + w / 2 - tw / 2, y + h / 2 - ui_px(7), font, c_text);
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

    DrawRectangle(x, y, w, h, ui_darken(c_bg, 12));
    ui_draw_bevel(x, y, w, h, ui_darken(c_bg, 45), ui_lighten(c_bg, 35));
    DrawTexturePro(texture, src, dst, (Vector2){0}, 0, WHITE);
}

/* ================================================================
 * MODAL DIALOGS
 * ================================================================ */

int
ui_draw_modal(InbeApp *app, const char *title, const char *message,
               const char *cancel_btn, const char *confirm_btn)
{
    int modal_w = ui_px(280);
    int modal_h = ui_px(160);
    int modal_x = (ui_view_width - modal_w) / 2;
    int modal_y = (ui_view_height - modal_h) / 2;
    int title_font = ui_clamp_px(16, 14, 18);
    int msg_font = ui_clamp_px(14, 12, 16);
    int btn_font = ui_clamp_px(14, 12, 16);
    int btn_h = ui_clamp_px(36, 32, 40);
    int btn_w = ui_px(100);
    int btn_gap = ui_px(12);
    int title_h = ui_px(32);
    int msg_y = modal_y + title_h;
    int btn_y = modal_y + modal_h - btn_h - ui_px(16);

    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
    int mx = (int)mouse_world.x;
    int my = (int)mouse_world.y;

    /* Dim background */
    DrawRectangle(0, 0, ui_view_width, ui_view_height, (Color){0, 0, 0, 180});

    /* Modal background */
    DrawRectangle(modal_x, modal_y, modal_w, modal_h, c_button);
    ui_draw_bevel(modal_x, modal_y, modal_w, modal_h, ui_lighten(c_button, 40), ui_darken(c_button, 40));

    /* Title */
    int title_w = MeasureText(title, title_font);
    DrawText(title, modal_x + (modal_w - title_w) / 2, modal_y + ui_px(12), title_font, c_text);

    /* Message (word wrap) */
    const char *msg = message;
    int msg_x = modal_x + ui_px(16);
    int msg_w = modal_w - ui_px(32);
    int line_y = msg_y;
    char word_buf[64];
    int word_len = 0;
    int line_w = 0;
    int msg_lines = 0;

    while(*msg && msg_lines < 4) {
        if(*msg == ' ' || *msg == '\0' || *msg == '\n') {
            if(word_len > 0) {
                word_buf[word_len] = '\0';
                int word_w = MeasureText(word_buf, msg_font);
                if(line_w == 0 || line_w + word_w <= msg_w) {
                    DrawText(word_buf, msg_x + line_w, line_y, msg_font, c_text);
                    line_w += word_w + MeasureText(" ", msg_font);
                } else {
                    line_y += msg_font + ui_px(4);
                    DrawText(word_buf, msg_x, line_y, msg_font, c_text);
                    line_w = word_w;
                    msg_lines++;
                }
                word_len = 0;
            }
            if(*msg == '\n') {
                line_y += msg_font + ui_px(4);
                line_w = 0;
                msg_lines++;
            }
        } else if(word_len < (int)sizeof(word_buf) - 1) {
            word_buf[word_len++] = *msg;
        }
        msg++;
    }

    /* Draw any remaining word after loop ends */
    if(word_len > 0 && msg_lines < 4) {
        word_buf[word_len] = '\0';
        int word_w = MeasureText(word_buf, msg_font);
        if(line_w == 0 || line_w + word_w <= msg_w) {
            DrawText(word_buf, msg_x + line_w, line_y, msg_font, c_text);
        } else {
            line_y += msg_font + ui_px(4);
            DrawText(word_buf, msg_x, line_y, msg_font, c_text);
        }
    }

    /* Buttons */
    int cancel_x = modal_x + (modal_w - btn_w * 2 - btn_gap) / 2;
    int confirm_x = cancel_x + btn_w + btn_gap;
    int result = 0;

    /* Cancel button */
    if(mx >= cancel_x && mx < cancel_x + btn_w && my >= btn_y && my < btn_y + btn_h) {
        DrawRectangle(cancel_x, btn_y, btn_w, btn_h, c_button_hover);
        ui_draw_bevel(cancel_x, btn_y, btn_w, btn_h, ui_darken(c_button_hover, 40), ui_lighten(c_button_hover, 40));
        app->cursor_clickable = 1;
        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
            result = 1;
    } else {
        DrawRectangle(cancel_x, btn_y, btn_w, btn_h, c_button);
        ui_draw_bevel(cancel_x, btn_y, btn_w, btn_h, ui_lighten(c_button, 40), ui_darken(c_button, 40));
    }
    int cancel_text_w = MeasureText(cancel_btn, btn_font);
    DrawText(cancel_btn, cancel_x + (btn_w - cancel_text_w) / 2, btn_y + (btn_h - btn_font) / 2 - 1, btn_font, c_text);

    /* Confirm button */
    if(mx >= confirm_x && mx < confirm_x + btn_w && my >= btn_y && my < btn_y + btn_h) {
        DrawRectangle(confirm_x, btn_y, btn_w, btn_h, c_button_hover);
        ui_draw_bevel(confirm_x, btn_y, btn_w, btn_h, ui_darken(c_button_hover, 40), ui_lighten(c_button_hover, 40));
        app->cursor_clickable = 1;
        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
            result = 2;
    } else {
        DrawRectangle(confirm_x, btn_y, btn_w, btn_h, c_circle);
        ui_draw_bevel(confirm_x, btn_y, btn_w, btn_h, ui_lighten(c_circle, 40), ui_darken(c_circle, 40));
    }
    int confirm_text_w = MeasureText(confirm_btn, btn_font);
    DrawText(confirm_btn, confirm_x + (btn_w - confirm_text_w) / 2, btn_y + (btn_h - btn_font) / 2 - 1, btn_font, c_text);

    return result;
}

int
ui_draw_modal_3btn(InbeApp *app, const char *title, const char *message,
                    const char *left_btn, const char *middle_btn, const char *right_btn)
{
    int modal_w = ui_px(300);
    int modal_h = ui_px(160);
    int modal_x = (ui_view_width - modal_w) / 2;
    int modal_y = (ui_view_height - modal_h) / 2;
    int title_font = ui_clamp_px(16, 14, 18);
    int msg_font = ui_clamp_px(14, 12, 16);
    int btn_font = ui_clamp_px(14, 12, 16);
    int btn_h = ui_clamp_px(36, 32, 40);
    int btn_w = ui_px(90);
    int btn_gap = ui_px(8);
    int title_h = ui_px(32);
    int msg_y = modal_y + title_h;
    int btn_y = modal_y + modal_h - btn_h - ui_px(16);

    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
    int mx = (int)mouse_world.x;
    int my = (int)mouse_world.y;

    /* Dim background */
    DrawRectangle(0, 0, ui_view_width, ui_view_height, (Color){0, 0, 0, 180});

    /* Modal background */
    DrawRectangle(modal_x, modal_y, modal_w, modal_h, c_button);
    ui_draw_bevel(modal_x, modal_y, modal_w, modal_h, ui_lighten(c_button, 40), ui_darken(c_button, 40));

    /* Title */
    int title_w = MeasureText(title, title_font);
    DrawText(title, modal_x + (modal_w - title_w) / 2, modal_y + ui_px(12), title_font, c_text);

    /* Message (word wrap) - same as 2-button version */
    const char *msg = message;
    int msg_x = modal_x + ui_px(16);
    int msg_w = modal_w - ui_px(32);
    int line_y = msg_y;
    char word_buf[64];
    int word_len = 0;
    int line_w = 0;
    int msg_lines = 0;

    while(*msg && msg_lines < 4) {
        if(*msg == ' ' || *msg == '\0' || *msg == '\n') {
            if(word_len > 0) {
                word_buf[word_len] = '\0';
                int word_w = MeasureText(word_buf, msg_font);
                if(line_w == 0 || line_w + word_w <= msg_w) {
                    DrawText(word_buf, msg_x + line_w, line_y, msg_font, c_text);
                    line_w += word_w + MeasureText(" ", msg_font);
                } else {
                    line_y += msg_font + ui_px(4);
                    DrawText(word_buf, msg_x, line_y, msg_font, c_text);
                    line_w = word_w;
                    msg_lines++;
                }
                word_len = 0;
            }
            if(*msg == '\n') {
                line_y += msg_font + ui_px(4);
                line_w = 0;
                msg_lines++;
            }
        } else if(word_len < (int)sizeof(word_buf) - 1) {
            word_buf[word_len++] = *msg;
        }
        msg++;
    }

    /* Draw any remaining word after loop ends */
    if(word_len > 0 && msg_lines < 4) {
        word_buf[word_len] = '\0';
        int word_w = MeasureText(word_buf, msg_font);
        if(line_w == 0 || line_w + word_w <= msg_w) {
            DrawText(word_buf, msg_x + line_w, line_y, msg_font, c_text);
        } else {
            line_y += msg_font + ui_px(4);
            DrawText(word_buf, msg_x, line_y, msg_font, c_text);
        }
    }

    /* Calculate button positions */
    int total_btn_w = btn_w * 3 + btn_gap * 2;
    int left_x = modal_x + (modal_w - total_btn_w) / 2;
    int middle_x = left_x + btn_w + btn_gap;
    int right_x = middle_x + btn_w + btn_gap;

    int result = 0;

    /* Left button (Cancel) */
    if(mx >= left_x && mx < left_x + btn_w && my >= btn_y && my < btn_y + btn_h) {
        DrawRectangle(left_x, btn_y, btn_w, btn_h, c_button_hover);
        ui_draw_bevel(left_x, btn_y, btn_w, btn_h, ui_darken(c_button_hover, 40), ui_lighten(c_button_hover, 40));
        app->cursor_clickable = 1;
        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
            result = 1;
    } else {
        DrawRectangle(left_x, btn_y, btn_w, btn_h, c_button);
        ui_draw_bevel(left_x, btn_y, btn_w, btn_h, ui_lighten(c_button, 40), ui_darken(c_button, 40));
    }
    int left_text_w = MeasureText(left_btn, btn_font);
    DrawText(left_btn, left_x + (btn_w - left_text_w) / 2, btn_y + (btn_h - btn_font) / 2 - 1, btn_font, c_text);

    /* Middle button (Save) - primary action */
    if(mx >= middle_x && mx < middle_x + btn_w && my >= btn_y && my < btn_y + btn_h) {
        DrawRectangle(middle_x, btn_y, btn_w, btn_h, c_button_hover);
        ui_draw_bevel(middle_x, btn_y, btn_w, btn_h, ui_darken(c_button_hover, 40), ui_lighten(c_button_hover, 40));
        app->cursor_clickable = 1;
        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
            result = 2;
    } else {
        DrawRectangle(middle_x, btn_y, btn_w, btn_h, c_circle);
        ui_draw_bevel(middle_x, btn_y, btn_w, btn_h, ui_lighten(c_circle, 40), ui_darken(c_circle, 40));
    }
    int middle_text_w = MeasureText(middle_btn, btn_font);
    DrawText(middle_btn, middle_x + (btn_w - middle_text_w) / 2, btn_y + (btn_h - btn_font) / 2 - 1, btn_font, c_text);

    /* Right button (Discard) */
    if(mx >= right_x && mx < right_x + btn_w && my >= btn_y && my < btn_y + btn_h) {
        DrawRectangle(right_x, btn_y, btn_w, btn_h, c_button_hover);
        ui_draw_bevel(right_x, btn_y, btn_w, btn_h, ui_darken(c_button_hover, 40), ui_lighten(c_button_hover, 40));
        app->cursor_clickable = 1;
        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
            result = 3;
    } else {
        DrawRectangle(right_x, btn_y, btn_w, btn_h, c_button);
        ui_draw_bevel(right_x, btn_y, btn_w, btn_h, ui_lighten(c_button, 40), ui_darken(c_button, 40));
    }
    int right_text_w = MeasureText(right_btn, btn_font);
    DrawText(right_btn, right_x + (btn_w - right_text_w) / 2, btn_y + (btn_h - btn_font) / 2 - 1, btn_font, c_text);

    return result;
}

/* ================================================================
 * SCREEN HEADER (TITLE BAR)
 * ================================================================ */

int
ui_screen_header_height(void)
{
    return ui_clamp_px(60, 48, 60);
}

int
ui_draw_screen_header(InbeApp *app, const char *title, int show_close)
{
    (void)c_bg; /* unused */
    int title_h = ui_screen_header_height();
    int title_font = ui_clamp_px(16, 14, 18);
    int close_hover = 0;
    int close_clicked = 0;

    /* Draw header background */
    DrawRectangle(0, 0, ui_view_width, title_h, ui_darken(c_bg, 14));
    DrawLine(0, title_h - 1, ui_view_width, title_h - 1, ui_darken(c_bg, 42));

    /* Draw centered title */
    int title_w = MeasureText(title, title_font);
    int title_y = (title_h - title_font) / 2;
    DrawText(title, (ui_view_width - title_w) / 2, title_y, title_font, c_text);

    /* Draw close button if requested */
    if(show_close) {
        close_clicked = ui_draw_icon_btn(app, ui_view_width - ui_px(40) - ui_icon_btn_padding(UI_ICON_SIZE_TINY), ui_px(8),
                                         UI_ICON_SIZE_TINY, app->x_icon, &close_hover);
    }

    return close_clicked;
}

/* ================================================================
 * END OF UI FUNCTIONS
 * ================================================================ */
