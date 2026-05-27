#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Constants */
enum {
    ICON_SIZE_SMALL = 22,
    ICON_SIZE_MEDIUM = 26,
    ICON_SIZE_LARGE = 30,
    ICON_SIZE_SMALL_MIN = 20,
    ICON_SIZE_SMALL_MAX = 36,
    ICON_SIZE_MEDIUM_MIN = 24,
    ICON_SIZE_MEDIUM_MAX = 40,
    ICON_SIZE_LARGE_MIN = 28,
    ICON_SIZE_LARGE_MAX = 44
};

/* Global UI state */
static float dpi_scale = 1.0f;
static int view_width = 320;
static int view_height = 560;
static Color c_text, c_bg, c_circle, c_button, c_button_hover, c_icon;

/* ================================================================
 * INITIALIZATION
 * ================================================================ */

void
ui_init(int width, int height, float dpi)
{
    view_width = width;
    view_height = height;
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

    int available_w = view_width - side_pad * 2;

    if(available_w < 0)
        available_w = 0;
    if(max_w > available_w)
        max_w = available_w;
    if(max_w < 0)
        max_w = 0;

    if(x != NULL)
        *x = (view_width - max_w) / 2;
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
        if(released) {
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
ui_draw_icon_btn_small(InbeApp *app, int x, int y, Texture2D icon, int *hover)
{
    return ui_draw_icon_btn(app, x, y, UI_ICON_SIZE_SMALL, icon, hover);
}

int
ui_draw_icon_btn_medium(InbeApp *app, int x, int y, Texture2D icon, int *hover)
{
    return ui_draw_icon_btn(app, x, y, UI_ICON_SIZE_MEDIUM, icon, hover);
}

int
ui_draw_icon_btn_large(InbeApp *app, int x, int y, Texture2D icon, int *hover)
{
    return ui_draw_icon_btn(app, x, y, UI_ICON_SIZE_LARGE, icon, hover);
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

    if(hover && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        OpenURL(url);
    }
}

/* ================================================================
 * CONTROLS
 * ================================================================ */

static int
clampi(int value, int min, int max)
{
    if(value < min)
        return min;
    if(value > max)
        return max;
    return value;
}

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
        if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
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
    int hover = 0;
    Rectangle bounds = {x, y, w, h};

    if(CheckCollisionPointRec(GetMousePosition(), bounds)) {
        hover = 1;
        app->cursor_clickable = 1;
    }

    int pressed = hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

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

    int circle_size = h - 10;
    int circle_x = *value ? x + w - circle_size / 2 - 5 : x + circle_size / 2 + 5;
    int circle_y = y + h / 2;
    DrawCircle(circle_x, circle_y, circle_size / 2, c_button);

    int font = ui_clamp_px(12, 10, 14);
    const char *light_label = "Light";
    const char *dark_label = "Dark";
    int light_w = MeasureText(light_label, font);
    int dark_w = MeasureText(dark_label, font);

    Color label_color = c_text;
    DrawText(light_label, x + 10 + active_w / 2 - light_w / 2, y + h / 2 - font / 2 - 1, font, label_color);
    DrawText(dark_label, x + w - 10 - active_w / 2 - dark_w / 2, y + h / 2 - font / 2 - 1, font, label_color);

    return pressed;
}

int
ui_draw_checkbox_toggle(InbeApp *app, int x, int y, const char *label, int *value)
{
    int font = ui_clamp_px(14, 12, 16);
    int box_size = ui_px(22);
    int hover = 0;
    Rectangle bounds = {x, y, box_size, box_size};

    if(CheckCollisionPointRec(GetMousePosition(), bounds)) {
        hover = 1;
        app->cursor_clickable = 1;
    }

    int pressed = hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
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

void
ui_draw_scrollbar(InbeApp *app, int *scroll, int content_h, int viewport_h,
                  int *drag_scrollbar, int *drag_content, int *drag_content_y)
{
    if(content_h <= viewport_h)
        return;

    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
    int bar_w = ui_px(8);
    int bar_x = view_width - bar_w;
    int bar_y = 0;
    int bar_h = viewport_h;
    int thumb_h = (viewport_h * bar_h) / content_h;
    int min_thumb_h = ui_px(32);
    if(thumb_h < min_thumb_h)
        thumb_h = min_thumb_h;
    int max_scroll = content_h - viewport_h;
    int thumb_y = bar_y + (*scroll * (bar_h - thumb_h)) / max_scroll;
    int hit_w = ui_px(16);
    Rectangle thumb = {(float)(bar_x - (hit_w - bar_w) / 2), (float)thumb_y, (float)hit_w, (float)thumb_h};
    Rectangle content_area = {(float)0, (float)bar_y, (float)bar_x, (float)bar_h};

    DrawRectangle(bar_x, bar_y, bar_w, bar_h, ui_darken(c_bg, 18));
    DrawRectangle(bar_x, thumb_y, bar_w, thumb_h, c_button_hover);

    if(CheckCollisionPointRec(mouse_world, thumb)) {
        app->cursor_clickable = 1;
        if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            *drag_scrollbar = 1;
    }

    if(*drag_scrollbar && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        int usable = bar_h - thumb_h;
        int y = (int)mouse_world.y - bar_y - thumb_h / 2;
        y = clampi(y, 0, usable);
        *scroll = (y * max_scroll) / usable;
        return;
    }

    if(CheckCollisionPointRec(mouse_world, content_area)) {
        if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            *drag_content = 1;
            *drag_content_y = (int)mouse_world.y;
        }
    }

    if(*drag_content && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        int delta = *drag_content_y - (int)mouse_world.y;
        *scroll += delta;
        *scroll = clampi(*scroll, 0, max_scroll);
        *drag_content_y = (int)mouse_world.y;
    }

    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        *drag_scrollbar = 0;
        *drag_content = 0;
    }
}

/* ================================================================
 * NAVIGATION
 * ================================================================ */

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
        Rectangle dst = {x + padding, y + padding, (float)icon_size, (float)icon_size};
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
    int bar_y = view_height - bar_h;
    int button_size = ui_clamp_px(ICON_SIZE_LARGE, ICON_SIZE_LARGE_MIN, ICON_SIZE_LARGE_MAX);
    int button_h = button_size + ui_px(12);
    int font = ui_clamp_px(14, 12, 16);
    int side_margin = ui_px(16);
    int group_gap = ui_px(10);
    int available_w = view_width - side_margin * 2;

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

    int group_x = side_margin + (available_w - (show_labels ? group_w_label : group_w_no_label)) / 2;
    int button_y = bar_y + (bar_h - button_h) / 2;
    int tab_hover = 0;

    DrawRectangle(0, bar_y, view_width, bar_h, ui_darken(c_bg, 10));
    DrawLine(0, bar_y, view_width, bar_y, ui_darken(c_bg, 42));

    int x = group_x;
    for(int i = 0; i < count; i++) {
        int w = ui_nav_button_width(tabs[i].label, button_size, show_labels, font);

        if(tabs[i].icon.id != 0) {
            if(ui_draw_nav_button(app, x, button_y, button_size, tabs[i].icon,
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
