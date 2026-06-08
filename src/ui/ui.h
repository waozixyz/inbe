#ifndef INBE_UI_H
#define INBE_UI_H

#include "raylib.h"
#include "app.h"
#include "flint.h"

typedef enum {
    UI_ICON_SIZE_TINY,
    UI_ICON_SIZE_SMALL,
    UI_ICON_SIZE_MEDIUM,
    UI_ICON_SIZE_LARGE
} UIIconSize;

typedef enum {
    UI_ICON_TYPE_NONE,
    UI_ICON_TYPE_GEAR,
    UI_ICON_TYPE_X,
    UI_ICON_TYPE_MANUAL,
    UI_ICON_TYPE_RETURN,
    UI_ICON_TYPE_BACKWARD,
    UI_ICON_TYPE_FORWARD,
    UI_ICON_TYPE_PLAY,
    UI_ICON_TYPE_PAUSE,
    UI_ICON_TYPE_STAT,
    UI_ICON_TYPE_TELEGRAM,
    UI_ICON_TYPE_GLOBE,
    UI_ICON_TYPE_STRIPE,
    UI_ICON_TYPE_MONERO,
    UI_ICON_TYPE_HOME,
    UI_ICON_TYPE_TRASH,
    UI_ICON_TYPE_PENCIL,
    UI_ICON_TYPE_SAVE
} UIIconType;

void ui_init(int width, int height, float dpi);
void ui_set_colors(Color text, Color bg, Color circle, Color button, Color button_hover, Color icon);
/* DPI scaling, color, and layout functions now from Flint: flint_px, flint_clamp_px, flint_lighten, flint_darken, flint_centered_column, flint_page_side_padding */
void ui_draw_bevel(int x, int y, int w, int h, Color light, Color dark);
void ui_draw_text_lines(const char **lines, int count, int x, int *y, int font, int line_h, Color color);
/* Icon fallback drawing now from Flint: flint_draw_icon_fallback */
int ui_icon_btn_size(UIIconSize size);
int ui_icon_btn_padding(UIIconSize size);
int ui_draw_icon_btn(InbeApp *app, int x, int y, UIIconSize size, Texture2D icon, UIIconType icon_type, int *hover);
int ui_draw_icon_btn_padded(InbeApp *app, int x, int y, int size, int padding, Texture2D icon, UIIconType icon_type, int *hover);
int ui_draw_text_btn(InbeApp *app, int x, int y, const char *label, int *hover);
void ui_draw_icon_link(InbeApp *app, int x, int y, int icon_size, Texture2D icon, UIIconType icon_type, const char *url);
int ui_draw_slider(InbeApp *app, int id, int x, int y, int w, const char *label, int min, int max, int *value, const char *suffix);
int ui_draw_toggle_switch(InbeApp *app, int x, int y, int w, int h, int *value,
                         const char *off_label, const char *on_label);
int ui_draw_checkbox_toggle(InbeApp *app, int x, int y, const char *label, int *value);
int ui_draw_dropdown_button(InbeApp *app, int id, int x, int y, int w, int h, const char **options, int option_count, int *selected_index);
int ui_draw_dropdown_menu(InbeApp *app, int id);
int ui_dropdown_captures_click(Vector2 point);
typedef struct UITab {
    const char *label;
    Texture2D icon;
    UIIconType icon_type;
    void (*on_click)(void *user_data);
    void *user_data;
} UITab;

typedef struct UITabBar {
    UITab *tabs;
    int count;
} UITabBar;

int ui_nav_button_width(const char *label, int icon_size, int show_label, int font);
int ui_draw_nav_button(InbeApp *app, int x, int y, int icon_size, Texture2D icon, UIIconType icon_type, const char *label, int show_label, int *hover);
int ui_draw_nav_button_expand(InbeApp *app, int x, int y, int icon_size, int w, Texture2D icon, UIIconType icon_type, const char *label, int show_label, int *hover);
void ui_draw_tab_bar(UITab *tabs, int count, InbeApp *app);
void ui_draw_tutorial_image_placeholder(const char *label, int x, int y, int w, int h);
void ui_draw_tutorial_image(Texture2D texture, const char *fallback, int x, int y, int w, int h);
int ui_draw_modal(InbeApp *app, const char *title, const char *message, const char *cancel_btn, const char *confirm_btn);
int ui_draw_modal_3btn(InbeApp *app, const char *title, const char *message, const char *left_btn, const char *middle_btn, const char *right_btn);
int ui_draw_screen_header(InbeApp *app, const char *title, int show_close);
int ui_screen_header_height(void);
int ui_draw_scrollbar(InbeApp *app, int x, int y, int viewport_h, int content_h, int *scroll_offset, int max_scroll);

extern int ui_view_height;
extern int ui_view_width;

#endif
