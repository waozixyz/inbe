#include "ui.h"
#include "flint_layout.h"

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

#define MAX_DROPDOWNS 8
static UIDropdownState dropdown_states[MAX_DROPDOWNS];
static int dropdown_state_count = 0;
static int dropdown_clip_top = 0;

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

static const FlintThemeId theme_picker_order[FLINT_THEME_COUNT] = {
    FLINT_THEME_SKY,
    FLINT_THEME_OCEAN,
    FLINT_THEME_COBALT,
    FLINT_THEME_FOREST,
    FLINT_THEME_SAGE,
    FLINT_THEME_MINT,
    FLINT_THEME_SUNSET,
    FLINT_THEME_INK,
    FLINT_THEME_DAWN,
    FLINT_THEME_CHERRY,
    FLINT_THEME_LAVENDER,
    FLINT_THEME_MONO
};

static UIThemeGridLayout
ui_theme_grid_layout(int w)
{
    UIThemeGridLayout layout = {0};
    int small_font = FLINT_TEXT_12;

    int row_gap = flint_px(14);
    layout.circle_size = flint_px(24);
    layout.label_gap = flint_px(6);
    layout.col_gap = flint_px(10);
    layout.cell_w = layout.circle_size;
    for(int i = 0; i < FLINT_THEME_COUNT; i++) {
        int name_w = flint_text_measure(flint_theme_label((FlintThemeId)i), small_font) + flint_px(8);
        if(name_w > layout.cell_w)
            layout.cell_w = name_w;
    }

    layout.per_row = (w + layout.col_gap) / (layout.cell_w + layout.col_gap);
    if(layout.per_row < 1)
        layout.per_row = 1;
    if(layout.per_row > 3)
        layout.per_row = 3;
    if(layout.per_row > FLINT_THEME_COUNT)
        layout.per_row = FLINT_THEME_COUNT;
    layout.row_width = layout.per_row * layout.cell_w + (layout.per_row - 1) * layout.col_gap;

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
    int small_font = FLINT_TEXT_12;
    int selected = theme_id != NULL ? *theme_id : FLINT_THEME_SKY;
    UIThemeGridLayout layout = ui_theme_grid_layout(w);
    int start_x = x + (w - layout.row_width) / 2;
    Vector2 mouse_world = ui_mouse_world();

    if(selected < 0 || selected >= FLINT_THEME_COUNT)
        selected = FLINT_THEME_SKY;

    for(int i = 0; i < FLINT_THEME_COUNT; i++) {
        FlintThemeId theme = theme_picker_order[i];
        int row = i / layout.per_row;
        int col = i % layout.per_row;
        int cell_x = start_x + col * (layout.cell_w + layout.col_gap);
        int cx = cell_x + layout.cell_w / 2;
        int cy = circle_y + row * layout.row_step;
        Color theme_color = c_circle;

        if(!flint_theme_catalog_color(theme, dark != 0, "circle", &theme_color)) {
            const char *scope = flint_theme_scope_for(theme, dark != 0);
            theme_color = flint_theme_get(scope, "circle");
        }

        DrawCircle(cx, cy, layout.circle_size / 2, theme_color);
        DrawCircleLines(cx, cy, layout.circle_size / 2 + (selected == theme ? flint_px(2) : flint_px(1)),
                        selected == theme ? c_text : flint_darken(c_bg, 30));

        Rectangle bounds = {
            (float)(cx - layout.circle_size / 2 - flint_px(4)),
            (float)(cy - layout.circle_size / 2 - flint_px(4)),
            (float)(layout.circle_size + flint_px(8)),
            (float)(layout.circle_size + flint_px(8))
        };
        if(CheckCollisionPointRec(mouse_world, bounds) && !ui_input_captures_click(mouse_world)) {
            ui_mark_clickable();
            if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                selected = theme;
                if(theme_id != NULL)
                    *theme_id = theme;
                changed = 1;
            }
        }

        const char *name = flint_theme_label(theme);
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
    int button_inside = CheckCollisionPointRec(mouse, btn_bounds);
    int hover = button_inside &&
                (state->open
                     ? !ui_base_input_captures_click(mouse, 1)
                     : !ui_input_captures_click(mouse));

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

    if(hover)
        ui_mark_clickable();

    /* Handle click on button */
    if(hover && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        state->open = !state->open;
        if(state->open) {
            state->just_opened = 1;
            state->scroll_offset = 0;
            state->touch_drag_active = 0;
        }
    }

    /* Draw button background */
    DrawRectangleRounded(btn_bounds, 0.3f, 8, ui_dropdown_panel_color(16));

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

int
ui_nav_button_width(const char *label, int icon_size, int show_label, int font)
{
    int padding = flint_px(6);
    int width = icon_size + padding * 2;

    if(show_label && label != NULL && label[0] != '\0')
        width += flint_px(10) + flint_text_measure(label, font);
    return width;
}

static int
ui_nav_button_height(const char *label, int icon_size, int show_label, int font)
{
    int padding = flint_px(6);
    int content_h = icon_size;

    if(show_label && label != NULL && label[0] != '\0') {
        int text_h = flint_text_height(label, font);
        if(text_h > content_h)
            content_h = text_h;
    }
    return content_h + padding * 2;
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
    int h = ui_nav_button_height(label, icon_size, show_label, font);
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
        Rectangle dst = {icon_x, y + (h - icon_size) / 2, (float)icon_size, (float)icon_size};
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
    int h = ui_nav_button_height(label, icon_size, show_label, font);
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
        Rectangle dst = {icon_x, y + (h - icon_size) / 2, (float)icon_size, (float)icon_size};
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

int
ui_draw_tab_bar(UITab *tabs, int count)
{
    int bar_h = flint_clamp_px(58, 54, 66);
    int bar_y = ui_view_height - bar_h;
    int button_size = flint_clamp_px(30, 28, 44);
    int font = flint_ui_font();
    int button_h = ui_nav_button_height("", button_size, 0, font);
    int side_margin = flint_px(16);
    int group_gap = flint_px(10);
    int available_w = ui_view_width - side_margin * 2;
    int label_safety_w = group_gap * (count + 1);

    /* Calculate widths with labels */
    int group_w_label = 0;
    for(int i = 0; i < count; i++) {
        group_w_label += ui_nav_button_width(tabs[i].label, button_size, 1, font);
        {
            int tab_h = ui_nav_button_height(tabs[i].label, button_size, 1, font);
            if(tab_h > button_h)
                button_h = tab_h;
        }
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
    if(!show_labels)
        button_h = ui_nav_button_height("", button_size, 0, font);
    if(button_h + flint_px(8) > bar_h)
        bar_h = button_h + flint_px(8);
    bar_y = ui_view_height - bar_h;

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
    int clicked = -1;

    DrawRectangle(0, bar_y, ui_view_width, bar_h, flint_darken(c_bg, 10));
    DrawLine(0, bar_y, ui_view_width, bar_y, flint_darken(c_bg, 42));

    int x = group_x;
    for(int i = 0; i < count; i++) {
        int base_w = ui_nav_button_width(tabs[i].label, button_size, show_labels, font);
        int w = base_w + extra_per_button;

        if(ui_draw_nav_button_expand(x, button_y, button_size, w, tabs[i].icon,
                                        tabs[i].label, show_labels, &tab_hover)) {
            clicked = i;
        }

        x += w + group_gap;
    }
    return clicked;
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
ui_scrollbar_reserved_width(int max_scroll)
{
    return max_scroll > 0 ? flint_px(16) : 0;
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

int
ui_scrollbar_safe_content_width(int content_x, int content_width,
                                int scrollbar_x, int max_scroll)
{
    int gap = flint_px(20);
    int safe_width = content_width;

    if(max_scroll <= 0 || scrollbar_x <= 0)
        return content_width;

    safe_width = scrollbar_x - content_x - gap;
    if(safe_width > content_width)
        return content_width;
    if(safe_width < 0)
        return 0;
    return safe_width;
}

FlintUIScrollView
ui_scroll_container_measure(FlintUIScrollArea area)
{
    FlintUIScrollView view;
    int x = (int)area.bounds.x;
    int y = (int)area.bounds.y;
    int w = (int)area.bounds.width;
    int h = (int)area.bounds.height;
    int scrollbar_w = flint_px(8);
    int scrollbar_x = area.scrollbar_x > 0
                          ? area.scrollbar_x
                          : x + w - scrollbar_w;
    int content_x = area.content_x > 0 ? area.content_x : x;
    int content_w = area.content_width > 0 ? area.content_width : w;

    memset(&view, 0, sizeof(view));
    view.content_x = content_x;
    view.viewport_h = h;
    view.content_h = area.content_height > 0 ? area.content_height : 0;
    view.max_scroll = view.content_h - h;
    if(view.max_scroll < 0)
        view.max_scroll = 0;

    if(area.scroll_offset != NULL) {
        int scroll_offset = ui_clampi(*area.scroll_offset, 0, view.max_scroll);
        view.content_y = y - scroll_offset;
    } else {
        view.content_y = y;
    }
    view.content_w = ui_scrollbar_safe_content_width(content_x, content_w,
                                                     scrollbar_x, view.max_scroll);

    return view;
}

FlintUIScrollPage
ui_scroll_page_begin(FlintUIScrollPageSpec spec)
{
    FlintUIScrollPage page;
    FlintUIScrollArea area;
    FlintUIScrollView measured;
    int max_content_w = spec.max_content_width;
    int min_content_w = spec.min_content_width;
    int side_padding = spec.side_padding > 0 ? spec.side_padding : flint_page_side_padding();
    int content_x = 0;
    int content_w = 0;
    int draw_w;
    int passes = spec.measure_passes > 0 ? spec.measure_passes : 3;

    memset(&page, 0, sizeof(page));

    if(max_content_w <= 0)
        max_content_w = ui_view_width;
    if(max_content_w > ui_view_width - side_padding * 2)
        max_content_w = ui_view_width - side_padding * 2;
    if(min_content_w > 0 && max_content_w < min_content_w)
        max_content_w = min_content_w;
    if(max_content_w < 0)
        max_content_w = 0;

    flint_centered_column(max_content_w, side_padding, &content_x, &content_w);
    draw_w = content_w;

    for(int i = 0; i < passes; i++) {
        int content_h;

        if(spec.content_height != NULL)
            content_h = spec.content_height(draw_w, spec.user_data);
        else
            content_h = 0;
        area = (FlintUIScrollArea){
            .bounds = {0.0f, (float)spec.y, (float)ui_view_width, (float)spec.height},
            .content_height = content_h,
            .content_x = content_x,
            .content_width = content_w,
            .scroll_offset = spec.scroll_offset,
            .wheel_step = spec.wheel_step > 0 ? spec.wheel_step : flint_px(42),
            .scrollbar_x = spec.scrollbar_x > 0 ? spec.scrollbar_x : ui_view_width - flint_px(8)
        };
        measured = ui_scroll_container_measure(area);
        if(measured.content_w == draw_w)
            break;
        draw_w = measured.content_w;
    }

    if(spec.content_height != NULL)
        area.content_height = spec.content_height(draw_w, spec.user_data);
    page.area = area;
    page.view = ui_scroll_container_begin(area);
    page.content_x = page.view.content_x;
    page.content_y = page.view.content_y;
    page.content_w = page.view.content_w;
    page.content_h = area.content_height;
    return page;
}

void
ui_scroll_page_end(FlintUIScrollPage page)
{
    ui_scroll_container_end(page.area, page.view);
}

int
ui_draw_icon_slider_popup(FlintUIIconSliderPopup popup)
{
    int hover = 0;
    int popup_w;
    int popup_h;
    int popup_x;
    int popup_y;
    Vector2 mouse;

    if(popup.open == NULL || popup.value == NULL)
        return 0;

    if(ui_draw_icon_btn_padded(popup.x, popup.y, popup.icon_size,
                               popup.icon_padding, popup.icon, &hover))
        *popup.open = !*popup.open;

    if(!*popup.open)
        return 0;

    popup_w = popup.popup_width > 0 ? popup.popup_width : flint_px(44);
    popup_h = popup.popup_height > 0 ? popup.popup_height : flint_px(200);
    popup_x = popup.x;
    popup_y = popup.y + popup.icon_size + popup.icon_padding * 2;
    mouse = ui_mouse_world();

    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
       (mouse.x < popup_x || mouse.x > popup_x + popup_w ||
        mouse.y < popup_y || mouse.y > popup_y + popup_h)) {
        *popup.open = 0;
        return 0;
    }

    DrawRectangle(popup_x, popup_y, popup_w, popup_h, c_surface);
    ui_draw_bevel(popup_x, popup_y, popup_w, popup_h,
                  flint_lighten(c_surface, 40), flint_darken(c_surface, 40));

    return ui_draw_slider_vertical(popup.id, popup_x + popup_w / 2,
                                   popup_y + flint_px(10),
                                   popup_h - flint_px(20),
                                   popup.min, popup.max, popup.value);
}

FlintUIIconRowResult
ui_draw_bottom_icon_row(FlintUIBottomIconRow row)
{
    FlintUIIconRowResult result = {-1, 0, 0};
    int count = row.count;
    int icon_size = row.icon_size > 0 ? row.icon_size : flint_px(24);
    int icon_padding = row.icon_padding > 0 ? row.icon_padding : flint_px(10);
    int gap = row.gap > 0 ? row.gap : flint_px(12);
    int side_margin = row.side_margin > 0 ? row.side_margin : flint_px(24);
    int bottom_margin = row.bottom_margin > 0 ? row.bottom_margin : flint_px(6);
    int min_icon_size = row.min_icon_size > 0 ? row.min_icon_size : flint_px(16);
    int min_icon_padding = row.min_icon_padding > 0 ? row.min_icon_padding : flint_px(6);
    int min_gap = row.min_gap > 0 ? row.min_gap : flint_px(8);
    int available_w;
    int max_btn_w;
    int button_w;
    int row_w;
    int start_x;

    if(row.items == NULL || count <= 0)
        return result;

    available_w = row.view_width - side_margin * 2;
    if(available_w < flint_px(120))
        available_w = flint_px(120);

    max_btn_w = row.max_button_width > 0 ? row.max_button_width : available_w;
    if(count > 1) {
        int fit_btn_w = (available_w - gap * (count - 1)) / count;
        if(max_btn_w <= 0 || max_btn_w > fit_btn_w)
            max_btn_w = fit_btn_w;
    }

    button_w = icon_size + icon_padding * 2;
    if(button_w > max_btn_w) {
        button_w = max_btn_w;
        icon_padding = button_w / 4;
        icon_size = button_w - icon_padding * 2;
    }

    if(icon_padding < min_icon_padding)
        icon_padding = min_icon_padding;
    if(icon_size < min_icon_size)
        icon_size = min_icon_size;

    button_w = icon_size + icon_padding * 2;
    gap = button_w / 4;
    if(gap < min_gap)
        gap = min_gap;

    row_w = button_w * count + gap * (count - 1);
    start_x = row.center_x - row_w / 2;
    result.y = row.view_height - bottom_margin - button_w;
    result.button_width = button_w;

    for(int i = 0; i < count; i++) {
        int hover = 0;
        int x = start_x + i * (button_w + gap);

        if(row.items[i].disabled)
            continue;
        if(ui_draw_icon_btn_padded(x, result.y, icon_size, icon_padding,
                                   row.items[i].icon, &hover))
            result.clicked_index = i;
    }

    return result;
}

FlintUIToolbarResult
ui_draw_toolbar(FlintUIToolbar toolbar)
{
    FlintUIToolbarResult result = {-1, -1};
    int side_padding = toolbar.side_padding > 0 ? toolbar.side_padding : flint_px(12);
    int action_icon_size = toolbar.action_icon_size > 0
                               ? toolbar.action_icon_size
                               : flint_px(20);
    int action_icon_padding = toolbar.action_icon_padding > 0
                                  ? toolbar.action_icon_padding
                                  : flint_px(8);
    int action_gap = toolbar.action_gap > 0 ? toolbar.action_gap : flint_px(6);
    int action_w = action_icon_size + action_icon_padding * 2;
    int action_y = toolbar.y + (toolbar.height - action_w) / 2;
    int controls_x = toolbar.x + toolbar.width - side_padding;

    if(toolbar.draw_menu) {
        if(toolbar.options != NULL && toolbar.selected_index != NULL &&
           ui_draw_dropdown_menu(toolbar.id))
            result.selected_menu_item = toolbar.selected_index != NULL
                                            ? *toolbar.selected_index
                                            : -1;
        return result;
    }

    DrawRectangle(toolbar.x, toolbar.y, toolbar.width, toolbar.height,
                  flint_darken(c_bg, 14));
    DrawLine(toolbar.x, toolbar.y + toolbar.height - 1,
             toolbar.x + toolbar.width, toolbar.y + toolbar.height - 1,
             flint_darken(c_bg, 42));

    if(toolbar.actions != NULL && toolbar.action_count > 0) {
        for(int i = toolbar.action_count - 1; i >= 0; i--) {
            int hover = 0;
            int action_x;

            controls_x -= action_w;
            action_x = controls_x;
            if(!toolbar.actions[i].disabled &&
               ui_draw_icon_btn_padded(action_x, action_y, action_icon_size,
                                       action_icon_padding,
                                       toolbar.actions[i].icon, &hover))
                result.clicked_action = i;
            controls_x -= action_gap;
        }
    }

    if(toolbar.options != NULL && toolbar.option_count > 0 &&
       toolbar.selected_index != NULL) {
        int dropdown_h = toolbar.dropdown_height > 0
                             ? toolbar.dropdown_height
                             : flint_px(36);
        int dropdown_x = toolbar.x + side_padding;
        int dropdown_y = toolbar.y + (toolbar.height - dropdown_h) / 2;
        int dropdown_w = controls_x - dropdown_x;
        int dropdown_available_w;

        if(toolbar.action_count > 0)
            dropdown_w -= side_padding - action_gap;
        dropdown_available_w = dropdown_w;
        if(toolbar.dropdown_min_width > 0 && dropdown_w < toolbar.dropdown_min_width)
            dropdown_w = toolbar.dropdown_min_width;
        if(toolbar.dropdown_max_width > 0 && dropdown_w > toolbar.dropdown_max_width)
            dropdown_w = toolbar.dropdown_max_width;
        if(dropdown_available_w > 0 && dropdown_w > dropdown_available_w)
            dropdown_w = dropdown_available_w;
        if(dropdown_w < 0)
            dropdown_w = 0;
        ui_draw_dropdown_button(toolbar.id, dropdown_x, dropdown_y,
                                dropdown_w, dropdown_h,
                                toolbar.options, toolbar.option_count,
                                toolbar.selected_index);
    }

    return result;
}

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

FlintUIScrollView
ui_scroll_container_begin(FlintUIScrollArea area)
{
    static int content_drag_active = 0;
    static int content_dragging = 0;
    static int content_drag_start_y = 0;
    static int content_drag_start_scroll = 0;
    FlintUIScrollView view = ui_scroll_container_measure(area);
    Vector2 mouse_world = ui_mouse_world();
    int y = (int)area.bounds.y;
    int wheel_step = area.wheel_step > 0 ? area.wheel_step : flint_px(42);
    int inside = CheckCollisionPointRec(mouse_world, area.bounds);
    int captured = ui_input_captures_click(mouse_world);
    int drag_threshold = flint_px(5);

    if(area.scroll_offset != NULL) {
        *area.scroll_offset = ui_clampi(*area.scroll_offset, 0, view.max_scroll);
        if(view.max_scroll > 0 && inside && !captured) {
            float wheel = GetMouseWheelMove();
            if(wheel != 0.0f) {
                *area.scroll_offset -= (int)(wheel * (float)wheel_step);
                *area.scroll_offset = ui_clampi(*area.scroll_offset, 0, view.max_scroll);
            }
        }

        if(g_ui_slider_active_id != 0 &&
           g_ui_pointer_owner == UI_POINTER_OWNER_NONE &&
           g_ui_pointer_dragging &&
           ui_pointer_drag_is_horizontal())
            g_ui_pointer_owner = UI_POINTER_OWNER_HORIZONTAL_SLIDER;

        if(g_ui_pointer_owner == UI_POINTER_OWNER_HORIZONTAL_SLIDER ||
           g_ui_pointer_owner == UI_POINTER_OWNER_VERTICAL_SLIDER) {
            content_drag_active = 0;
            content_dragging = 0;
        }

        if(view.max_scroll > 0 &&
           g_ui_pointer_owner == UI_POINTER_OWNER_NONE &&
           IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && inside && !captured) {
            content_drag_active = 1;
            content_dragging = 0;
            content_drag_start_y = (int)mouse_world.y;
            content_drag_start_scroll = *area.scroll_offset;
        }
        if(content_drag_active && IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
           g_ui_pointer_owner != UI_POINTER_OWNER_HORIZONTAL_SLIDER &&
           g_ui_pointer_owner != UI_POINTER_OWNER_VERTICAL_SLIDER) {
            int dy = (int)mouse_world.y - content_drag_start_y;
            if(content_dragging || dy > drag_threshold || dy < -drag_threshold) {
                g_ui_pointer_owner = UI_POINTER_OWNER_SCROLL;
                content_dragging = 1;
                *area.scroll_offset = content_drag_start_scroll - dy;
                *area.scroll_offset = ui_clampi(*area.scroll_offset, 0, view.max_scroll);
                ui_set_input_blocked(1);
            }
        } else if(content_drag_active) {
            if(content_dragging)
                ui_set_input_blocked(1);
            content_drag_active = 0;
            content_dragging = 0;
        }
        view.content_y = y - *area.scroll_offset;
    } else {
        view.content_y = y;
    }
    {
        Rectangle screen_bounds = {
            g_ui_camera.offset.x + area.bounds.x * g_ui_camera.zoom,
            g_ui_camera.offset.y + area.bounds.y * g_ui_camera.zoom,
            area.bounds.width * g_ui_camera.zoom,
            area.bounds.height * g_ui_camera.zoom
        };
        Rectangle clipped_screen_bounds = flint_clip_effective(screen_bounds);
        Rectangle clipped_world_bounds = {
            (clipped_screen_bounds.x - g_ui_camera.offset.x) / g_ui_camera.zoom,
            (clipped_screen_bounds.y - g_ui_camera.offset.y) / g_ui_camera.zoom,
            clipped_screen_bounds.width / g_ui_camera.zoom,
            clipped_screen_bounds.height / g_ui_camera.zoom
        };

        ui_push_input_clip(clipped_world_bounds);
        flint_clip_begin((int)screen_bounds.x, (int)screen_bounds.y,
                         (int)screen_bounds.width, (int)screen_bounds.height);
    }
    return view;
}

void
ui_scroll_container_end(FlintUIScrollArea area, FlintUIScrollView view)
{
    int scrollbar_w = flint_px(8);
    int scrollbar_x;

    flint_clip_end();
    ui_pop_input_clip();

    if(area.scroll_offset == NULL || view.max_scroll <= 0)
        return;

    scrollbar_x = area.scrollbar_x > 0
                      ? area.scrollbar_x
                      : (int)(area.bounds.x + area.bounds.width) - scrollbar_w;
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
    int track_padding = 0;

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
    int input_captured = ui_input_captures_click_internal(mouse_pos, 0);
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

    DrawRectangle(x, y, scrollbar_width, viewport_h, flint_darken(c_bg, 20));

    Color thumb_color = thumb_hover || scrollbar_drag_active ? c_button_hover : c_button;
    DrawRectangleRec(thumb_bounds, thumb_color);

    return 1;
}

/* ================================================================
 * END OF UI FUNCTIONS
 * ================================================================ */
