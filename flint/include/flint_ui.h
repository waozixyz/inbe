#ifndef FLINT_UI_H
#define FLINT_UI_H

#include "raylib.h"
#include "flint.h"
#include "flint_text.h"
#include "ui_icon_types.h"

typedef enum {
    UI_ICON_SIZE_TINY,
    UI_ICON_SIZE_SMALL,
    UI_ICON_SIZE_MEDIUM,
    UI_ICON_SIZE_LARGE
} UIIconSize;

typedef enum {
    UI_BUTTON_STYLE_PRIMARY,
    UI_BUTTON_STYLE_SECONDARY,
    UI_BUTTON_STYLE_DANGER,
    UI_BUTTON_STYLE_TAB,
    UI_BUTTON_STYLE_TAB_SELECTED
} UIButtonStyle;

typedef struct {
    Color background;
    Color border;
    Color focus_border;
    Color text;
    Color cursor;
    float radius;
    int padding_x;
} FlintUITextInputStyle;

typedef struct {
    Rectangle bounds;
    const char *label;
    int font;
    int focus_id;
    int disabled;
    Color background;
    Color hover_background;
    Color text;
    Color border;
    float radius;
} FlintUIButton;

typedef struct {
    Rectangle bounds;
    Texture2D icon;
    UIIconType icon_type;
    int icon_size;
    int icon_padding;
    int focus_id;
    int disabled;
    Color background;
    Color hover_background;
    Color icon_color;
    Color border;
    float radius;
} FlintUIIconButton;

typedef struct {
    Rectangle bounds;
    const char *text;
    int cursor_position;
    int focused;
    int cursor_visible;
    int font;
    int focus_id;
    FlintUITextInputStyle style;
} FlintUITextInput;

typedef struct {
    const char *text;
    Texture2D icon;
    UIIconType icon_type;
    int icon_size;
    int width;
    int font;
    int line_gap;
    Color color;
} FlintUIParagraph;

typedef struct {
    const char *label;
    int disabled;
    Color accent;
} FlintUISubtab;

typedef struct {
    Rectangle bounds;
    const FlintUISubtab *tabs;
    int count;
    int selected_index;
    int font;
} FlintUISubtabBar;

typedef struct {
    Rectangle bounds;
    int content_height;
    int *scroll_offset;
    int wheel_step;
    int scrollbar_x;
} FlintUIScrollArea;

typedef struct {
    int content_x;
    int content_y;
    int content_w;
    int viewport_h;
    int content_h;
    int max_scroll;
} FlintUIScrollView;

typedef struct {
    int x;
    int y;
    int w;
    int h;
    int content_x;
    int content_y;
    int content_w;
    int content_h;
    int left_clicked;
    int right_clicked;
} FlintUIPanelFrame;

typedef struct {
    int height;
    int left_clicked;
    int right_clicked;
} FlintUIHeader;

void ui_init(int width, int height, float dpi);
void ui_set_colors(Color text, Color bg, Color surface, Color circle, Color button, Color button_hover, Color icon);
void ui_set_frame(Camera2D camera);
void ui_set_cursor_clickable(int *cursor_clickable);
void ui_set_cursor_disabled(int *cursor_disabled);
void ui_set_icons(Texture2D gear_icon, Texture2D x_icon);
void ui_set_input_blocked(int blocked);
int ui_input_captures_click(Vector2 point);
/* DPI scaling, color, and layout functions now from Flint: flint_px, flint_clamp_px, flint_lighten, flint_darken, flint_centered_column, flint_page_side_padding */
int flint_ui_font(void);
int flint_ui_font_small(void);
int flint_ui_text_y(const char *text, int box_y, int box_h, int font);
void flint_ui_draw_text_centered(const char *text, int center_x, int center_y, int font, Color color);
void flint_ui_draw_text_input(Rectangle bounds, const char *text, int cursor_position,
                              int focused, int cursor_visible, int font,
                              FlintUITextInputStyle style);
int flint_ui_button(FlintUIButton button);
int flint_ui_icon_button(FlintUIIconButton button);
int flint_ui_text_input(FlintUITextInput input);
int flint_ui_paragraph_height(FlintUIParagraph paragraph);
void flint_ui_paragraph_draw(FlintUIParagraph paragraph, int x, int *y);
void ui_draw_bevel(int x, int y, int w, int h, Color light, Color dark);
void ui_draw_text_lines(const char **lines, int count, int x, int *y, int font, int line_h, Color color);
/* Icon fallback drawing now from Flint: flint_draw_icon_fallback */
int ui_icon_btn_size(UIIconSize size);
int ui_icon_btn_padding(UIIconSize size);
int ui_draw_icon_btn(int x, int y, UIIconSize size, Texture2D icon, int *hover);
int ui_draw_icon_btn_padded(int x, int y, int size, int padding, Texture2D icon, int *hover);
int ui_draw_text_btn(int x, int y, const char *label, int *hover);

/* Generic button component with unified styling */
int ui_draw_generic_button(int x, int y, int w, int h, const char *label,
                           UIButtonStyle style, int disabled, int *hover);
int ui_draw_subtab_bar(FlintUISubtabBar bar);

void ui_draw_icon_link(int x, int y, int icon_size, Texture2D icon, const char *url);
int ui_draw_slider(int id, int x, int y, int w, const char *label, int min, int max, int *value, const char *suffix);
int ui_draw_slider_vertical(int id, int x, int y, int h,
                             int min, int max, int *value);
typedef void (*ui_slider_vertical_mark_callback)(void *user_data, int x, int y, int h, int min, int max, int value);
int ui_draw_slider_vertical_with_marks(int id, int x, int y, int h,
                                       int min, int max, int *value, ui_slider_vertical_mark_callback callback,
                                       void *callback_user_data);
int ui_draw_toggle_switch(int x, int y, int w, int h, int *value,
                         const char *off_label, const char *on_label);
int ui_draw_checkbox_toggle(int x, int y, const char *label, int *value);
int ui_draw_dropdown_button(int id, int x, int y, int w, int h, const char **options, int option_count, int *selected_index);
int ui_draw_dropdown_menu(int id);
int ui_dropdown_captures_click(Vector2 point);
void ui_set_dropdown_clip_top(int top);
int ui_draw_theme_switcher(int x, int y, int w, const char *label,
                           const char *light_label, const char *dark_label,
                           int *theme_id, int *dark_mode);
int ui_draw_theme_picker(int x, int y, int w, const char *label,
                         int dark_mode, int *theme_id);
int ui_theme_picker_height(int w);
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
int ui_draw_nav_button(int x, int y, int icon_size, Texture2D icon, const char *label, int show_label, int *hover);
int ui_draw_nav_button_expand(int x, int y, int icon_size, int w, Texture2D icon, const char *label, int show_label, int *hover);
void ui_draw_tab_bar(UITab *tabs, int count);
void ui_draw_tutorial_image_placeholder(const char *label, int x, int y, int w, int h);
void ui_draw_tutorial_image(Texture2D texture, const char *fallback, int x, int y, int w, int h);
int ui_draw_modal(const char *title, const char *message, const char *cancel_btn, const char *confirm_btn);
int ui_draw_modal_3btn(const char *title, const char *message, const char *left_btn, const char *middle_btn, const char *right_btn);
int ui_draw_screen_header(const char *title, int show_close);
int ui_screen_header_height(void);
FlintUIHeader ui_draw_title_header(int height, const char *title,
                                   Texture2D left_icon,
                                   Texture2D right_icon);
FlintUIPanelFrame ui_draw_modal_frame(int width, int height, const char *title,
                                      Texture2D left_icon,
                                      Texture2D right_icon);
void ui_begin_scissor(int x, int y, int w, int h);
void ui_end_scissor(void);
int ui_scrollbar_reserved_width(int max_scroll);
int ui_scrollbar_content_width(int content_width, int max_scroll);
FlintUIScrollView ui_scroll_container_begin(FlintUIScrollArea area);
void ui_scroll_container_end(FlintUIScrollArea area, FlintUIScrollView view);
int ui_draw_scrollbar(int x, int y, int viewport_h, int content_h, int *scroll_offset, int max_scroll);

void ui_focus_begin(void);
void ui_focus_end(void);
int ui_focus_register(int id, Rectangle bounds);
int ui_focus_is_active(int id);
int ui_focus_activate_pressed(int id);
void ui_focus_set(int id);
void ui_focus_clear(void);
void ui_focus_set_text_input_active(int active);
void ui_focus_draw(Rectangle bounds);

extern int ui_view_height;
extern int ui_view_width;

#endif
