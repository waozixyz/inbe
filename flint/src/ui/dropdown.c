#include "ui.h"


/* Per-dropdown state to track open/closed and click handling */
#define MAX_DROPDOWN_OPTIONS 128
#define DROPDOWN_OPTION_TEXT_SIZE 256

typedef struct UIDropdownState {
    int id;
    int open;
    int just_opened;
    int scroll_offset;
    int x, y, w, h;
    const char *options[MAX_DROPDOWN_OPTIONS];
    char option_text[MAX_DROPDOWN_OPTIONS][DROPDOWN_OPTION_TEXT_SIZE];
    int option_count;
    int *selected_index;
    int touch_pressed;
    int touch_press_start_y;
    int touch_drag_active;
} UIDropdownState;

#define MAX_DROPDOWNS 24
static UIDropdownState dropdown_states[MAX_DROPDOWNS];
static int dropdown_state_count = 0;
static int dropdown_clip_top = 0;
static int dropdown_clip_bottom = 0;

static Color
ui_dropdown_panel_color(int amount)
{
    int luminance = ((int)c_bg.r + (int)c_bg.g + (int)c_bg.b) / 3;
    return luminance < 96 ? flint_lighten(c_bg, amount) : flint_darken(c_bg, amount);
}

void
ui_set_dropdown_clip_top(int top)
{
    dropdown_clip_top = top > 0 ? top : 0;
}

void
ui_set_dropdown_clip_bottom(int bottom)
{
    dropdown_clip_bottom = bottom > 0 ? bottom : 0;
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
    int bottom_limit;
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
    bottom_limit = dropdown_clip_bottom > 0 ? dropdown_clip_bottom : ui_view_height;
    below_space = bottom_limit - below_y - flint_px(16);
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

int
ui_dropdown_captures_click(Vector2 point)
{
    (void)point;
    for(int i = 0; i < dropdown_state_count; i++) {
        UIDropdownState *state = &dropdown_states[i];
        if(state->open && state->option_count > 0)
            return 1;
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
    Color button_bg;
    int button_inside = CheckCollisionPointRec(mouse, btn_bounds);
    int active = button_inside &&
                 (state->open
                      ? !ui_base_input_captures_click(mouse, 1)
                      : !ui_input_captures_click(mouse));
    int hover = active && ui_hover_effects_enabled();

    /* Calculate arrow position */
    int arrow_x = x + w - arrow_pad;
    int arrow_y = y + h / 2;

    /* Store state for menu drawing */
    state->x = x;
    state->y = y;
    state->w = w;
    state->h = h;
    state->selected_index = selected_index;
    if(option_count < 0)
        option_count = 0;
    if(option_count > MAX_DROPDOWN_OPTIONS)
        option_count = MAX_DROPDOWN_OPTIONS;
    state->option_count = option_count;
    for(int i = 0; i < option_count; i++) {
        snprintf(state->option_text[i], sizeof(state->option_text[i]), "%s",
                 options != NULL && options[i] != NULL ? options[i] : "");
        state->options[i] = state->option_text[i];
    }

    if(active)
        ui_mark_clickable();

    /* Handle click on button */
    if(active && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        state->open = !state->open;
        if(state->open) {
            state->just_opened = 1;
            state->scroll_offset = 0;
            state->touch_drag_active = 0;
        }
    }

    /* Draw button background */
    button_bg = state->open ? ui_dropdown_panel_color(28)
                            : (hover ? c_button_hover : ui_dropdown_panel_color(16));
    DrawRectangleRec(btn_bounds, button_bg);
    ui_draw_bevel(x, y, w, h,
                  state->open ? flint_lighten(button_bg, 34) : flint_lighten(button_bg, 24),
                  state->open ? flint_darken(button_bg, 38) : flint_darken(button_bg, 30));

    /* Draw current selection text, clipped before the X icon. */
    int current_index = selected_index != NULL ? *selected_index : 0;
    if(current_index < 0 || current_index >= option_count)
        current_index = 0;
    const char *current_name = option_count > 0 ? state->options[current_index] : "";
    int text_x = x + flint_px(12);
    int text_w = arrow_x - arrow_size - flint_px(8) - text_x;
    if(text_w > 0) {
        flint_clip_begin((int)(g_ui_camera.offset.x + (float)text_x * g_ui_camera.zoom),
                         (int)(g_ui_camera.offset.y + (float)y * g_ui_camera.zoom),
                         (int)((float)text_w * g_ui_camera.zoom),
                         (int)((float)h * g_ui_camera.zoom));
        flint_text_draw(current_name, text_x, flint_ui_text_y(current_name, y, h, font), font, c_text);
        flint_clip_end();
    }

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
ui_draw_dropdown_menu(int id)
{
    UIDropdownState *state = get_or_create_dropdown_state(id);
    int changed = 0;

    if(!state->open || state->option_count <= 0 || state->selected_index == NULL)
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
    DrawRectangle(x, dropdown_y, w, dropdown_h, ui_dropdown_panel_color(18));
    ui_draw_bevel(x, dropdown_y, w, dropdown_h,
                  ui_dropdown_panel_color(32), ui_dropdown_panel_color(8));

    flint_clip_begin((int)(g_ui_camera.offset.x + (float)x * g_ui_camera.zoom),
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

        int option_active = CheckCollisionPointRec(mouse, option_bounds);
        int option_hover = option_active && ui_hover_effects_enabled();

        if(option_active) {
            if(option_hover)
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
                flint_clip_end();
                goto draw_arrow;
            }
        }

        flint_text_draw(options[i], x + flint_px(12), flint_ui_text_y(options[i], option_y, option_h, font), font, c_text);
    }

    flint_clip_end();

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

/* ================================================================
 * TUTORIAL HELPERS
 * ================================================================ */
