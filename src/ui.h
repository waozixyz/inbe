#ifndef INBE_UI_H
#define INBE_UI_H

#include "raylib.h"
#include "app.h"

/* Icon Size Enum */
typedef enum {
    UI_ICON_SIZE_TINY,
    UI_ICON_SIZE_SMALL,
    UI_ICON_SIZE_MEDIUM,
    UI_ICON_SIZE_LARGE
} UIIconSize;

/* ================================================================
 * INITIALIZATION
 * ================================================================ */

/* Initialize UI library with viewport dimensions and DPI scale */
void ui_init(int width, int height, float dpi);

/* Update theme colors used by UI library */
void ui_set_colors(Color text, Color bg, Color circle, Color button, Color button_hover, Color icon);

/* ================================================================
 * DPI SCALING
 * ================================================================ */

/* Convert logical pixels to physical pixels based on DPI scaling */
int ui_px(int px);

/* Convert logical pixels to physical pixels with min/max clamping */
int ui_clamp_px(int px, int min_px, int max_px);

/* ================================================================
 * COLOR HELPERS
 * ================================================================ */

/* Lighten a color by a specified amount */
Color ui_lighten(Color c, int amount);

/* Darken a color by a specified amount */
Color ui_darken(Color c, int amount);

/* ================================================================
 * LAYOUT HELPERS
 * ================================================================ */

/* Calculate centered column position and width */
void ui_centered_column(int max_w, int side_pad, int *x, int *w);

/* Draw a bevel border effect using rectangles */
void ui_draw_bevel(int x, int y, int w, int h, Color light, Color dark);

/* Draw multiple text lines with incremental y position */
void ui_draw_text_lines(const char **lines, int count, int x, int *y, int font, int line_h);

/* ================================================================
 * ICON BUTTONS
 * ================================================================ */

/* Get icon button size with DPI scaling */
int ui_icon_btn_size(UIIconSize size);

/* Get icon button padding */
int ui_icon_btn_padding(UIIconSize size);

/* Draw a standard icon button with consistent sizing */
int ui_draw_icon_btn(InbeApp *app, int x, int y, UIIconSize size, Texture2D icon, int *hover);

/* Convenience wrapper for small icon buttons */
int ui_draw_icon_btn_small(InbeApp *app, int x, int y, Texture2D icon, int *hover);

/* Convenience wrapper for medium icon buttons */
int ui_draw_icon_btn_medium(InbeApp *app, int x, int y, Texture2D icon, int *hover);

/* Convenience wrapper for large icon buttons */
int ui_draw_icon_btn_large(InbeApp *app, int x, int y, Texture2D icon, int *hover);

/* Draw an icon link button that opens a URL when clicked */
void ui_draw_icon_link(InbeApp *app, int x, int y, int icon_size, Texture2D icon, const char *url);

/* ================================================================
 * CONTROLS
 * ================================================================ */

/* Draw a slider control with label and value display */
int ui_draw_slider(InbeApp *app, int id, int x, int y, int w, const char *label,
                   int min, int max, int *value, const char *suffix);

/* Draw a toggle switch (typically for light/dark mode) */
int ui_draw_toggle_switch(InbeApp *app, int x, int y, int w, int h, int *value);

/* Draw a checkbox toggle with label */
int ui_draw_checkbox_toggle(InbeApp *app, int x, int y, const char *label, int *value);

/* Draw a scrollbar with thumb and handle content dragging */
void ui_draw_scrollbar(InbeApp *app, int *scroll, int content_h, int viewport_h,
                       int *drag_scrollbar, int *drag_content, int *drag_content_y);

/* ================================================================
 * NAVIGATION
 * ================================================================ */

/* Tab definition for tab bar */
typedef struct UITab {
    const char *label;
    Texture2D icon;
    void (*on_click)(void *user_data);
    void *user_data;
} UITab;

typedef struct UITabBar {
    UITab *tabs;
    int count;
} UITabBar;

/* Calculate width of navigation buttons with optional label */
int ui_nav_button_width(const char *label, int icon_size, int show_label, int font);

/* Draw a navigation button with icon and optional label */
int ui_draw_nav_button(InbeApp *app, int x, int y, int icon_size, Texture2D icon,
                       const char *label, int show_label, int *hover);

/* Draw the bottom tab bar - automatic, data-driven */
void ui_draw_tab_bar(UITab *tabs, int count, InbeApp *app);

/* ================================================================
 * TUTORIAL HELPERS
 * ================================================================ */

/* Draw a placeholder for tutorial images */
void ui_draw_tutorial_image_placeholder(const char *label, int x, int y, int w, int h);

/* Draw tutorial images with fallback to placeholder */
void ui_draw_tutorial_image(Texture2D texture, const char *fallback, int x, int y, int w, int h);

#endif
