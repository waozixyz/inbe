#ifndef FLINT_UI_H
#define FLINT_UI_H

#include "raylib.h"
#include "flint.h"
#include "flint_text.h"
#include "ui_icon_types.h"
#include <stddef.h>

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
    int id;
    int x;
    int y;
    int icon_size;
    int icon_padding;
    Texture2D icon;
    int *open;
    int *value;
    int min;
    int max;
    int popup_width;
    int popup_height;
} FlintUIIconSliderPopup;

typedef struct {
    Texture2D icon;
    int disabled;
} FlintUIIconRowItem;

typedef struct {
    int center_x;
    int view_width;
    int view_height;
    int count;
    const FlintUIIconRowItem *items;
    int icon_size;
    int icon_padding;
    int gap;
    int side_margin;
    int bottom_margin;
    int max_button_width;
    int min_icon_size;
    int min_icon_padding;
    int min_gap;
} FlintUIBottomIconRow;

typedef struct {
    int clicked_index;
    int y;
    int button_width;
} FlintUIIconRowResult;

typedef struct {
    int route;
    const char *label;
    Texture2D icon;
    int active;
    int disabled;
} FlintUIBottomNavItem;

typedef struct {
    int view_width;
    int view_height;
    int count;
    const FlintUIBottomNavItem *items;
    int height;
    int icon_size;
    int icon_padding;
    int side_margin;
    int bottom_margin;
    int max_button_width;
} FlintUIBottomNav;

typedef struct {
    int clicked_index;
    int clicked_route;
    int y;
    int height;
} FlintUIBottomNavResult;

typedef struct {
    int route;
    const char *label;
    Texture2D icon;
} FlintUIBottomNavOption;

typedef struct {
    int id;
    const char *title;
    int *routes;
    int *route_count;
    int max_route_count;
    const char **slot_labels;
    const FlintUIBottomNavOption *options;
    int option_count;
    const char *add_label;
    const char *cancel_label;
    const char *save_label;
    const char *reset_label;
    Texture2D close_icon;
} FlintUIBottomNavConfigModal;

typedef struct {
    int action;
    int changed;
} FlintUIBottomNavConfigResult;

typedef struct {
    const char *message;
    int width;
    int header_h;
    int button_h;
    int line_gap;
    int extra_lines;
    int min_height;
    int font;
} FlintUIParagraphModalMeasure;

typedef struct {
    Texture2D icon;
    int disabled;
} FlintUIToolbarAction;

typedef struct {
    int id;
    int x;
    int y;
    int width;
    int height;
    int draw_menu;
    const char **options;
    int option_count;
    int *selected_index;
    int dropdown_min_width;
    int dropdown_max_width;
    int dropdown_height;
    const FlintUIToolbarAction *actions;
    int action_count;
    int action_icon_size;
    int action_icon_padding;
    int action_gap;
    int side_padding;
} FlintUIToolbar;

typedef struct {
    int selected_menu_item;
    int clicked_action;
} FlintUIToolbarResult;

typedef struct {
    FlintUIToolbar toolbar;
    Texture2D leading_icon;
    int leading_width;
    int leading_icon_size;
    int leading_icon_padding;
} FlintUIToolbarHeader;

typedef struct {
    FlintUIToolbarResult toolbar;
    int leading_clicked;
} FlintUIToolbarHeaderResult;

typedef struct {
    const char *text;
    int font;
    Color color;
} FlintUIInfoRow;

typedef struct {
    int x;
    int y;
    int width;
    int row_height;
    int padding_x;
    const FlintUIInfoRow *rows;
    int row_count;
    Color background;
    Color separator;
    Color default_text;
} FlintUIInfoRows;

typedef struct {
    const char *label;
    UIButtonStyle style;
    int disabled;
} FlintUIButtonRowItem;

typedef struct {
    int x;
    int y;
    int width;
    int height;
    int gap;
    const FlintUIButtonRowItem *items;
    int count;
} FlintUIButtonRow;

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

typedef int (*FlintUITextInputFilter)(int codepoint, void *user_data);

typedef struct {
    char *text;
    size_t text_size;
    int *cursor_position;
    int max_codepoints;
    FlintUITextInputFilter filter;
    void *filter_user_data;
    int *commit_pressed;
} FlintUITextEdit;

typedef struct {
    Rectangle bounds;
    char *text;
    size_t text_size;
    int *cursor_position;
    int *focused;
    int max_codepoints;
    int font;
    int focus_id;
    FlintUITextInputStyle style;
    FlintUITextInputFilter filter;
    void *filter_user_data;
    int *commit_pressed;
} FlintUITextField;

typedef struct {
    const char *label;
    FlintUITextField field;
    int label_font;
    int label_h;
    int field_h;
    int gap;
    int bottom_gap;
    Color label_color;
} FlintUILabelTextField;

typedef struct {
    const char *label;
    int font;
    int info_button;
    int icon_diameter;
    int height;
    Color color;
} FlintUISectionLabel;

typedef struct {
    const char *label;
    int *value;
    int height;
    int disabled;
} FlintUICheckboxRow;

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
    Texture2D icon;
    int icon_size;
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
    const char *label;
    Texture2D icon;
    int icon_size;
    int disabled;
    Color accent;
} FlintUITab;

typedef struct {
    Rectangle bounds;
    const FlintUITab *tabs;
    int count;
    int selected_index;
    int font;
    int min_tab_width;
    int max_tab_width;
    int *scroll_offset;
    int focus_selected;
} FlintUITabBar;

typedef struct {
    Rectangle anchor;
    const char *text;
} FlintUIGuideStep;

typedef struct {
    const FlintUIGuideStep *steps;
    int count;
    int *step;
    int view_width;
    int view_height;
    int reserved_top;
    int reserved_bottom;
    int max_width;
    int line_gap;
    int paragraph_font;
    Texture2D close_icon;
    Texture2D back_icon;
    Texture2D next_icon;
    Texture2D done_icon;
} FlintUIGuideOverlay;

typedef struct {
    int closed;
    int finished;
    int changed;
    int step;
} FlintUIGuideResult;

typedef struct {
    Rectangle bounds;
    int content_height;
    int content_x;
    int content_width;
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

typedef int (*FlintUIScrollPageHeightFn)(int content_width, void *user_data);

typedef struct {
    int y;
    int height;
    int max_content_width;
    int min_content_width;
    int side_padding;
    int *scroll_offset;
    int wheel_step;
    int scrollbar_x;
    int measure_passes;
    FlintUIScrollPageHeightFn content_height;
    void *user_data;
} FlintUIScrollPageSpec;

typedef struct {
    FlintUIScrollArea area;
    FlintUIScrollView view;
    int content_x;
    int content_y;
    int content_w;
    int content_h;
} FlintUIScrollPage;

typedef void (*FlintUITextInputPlatformCallback)(int active);

void ui_init(int width, int height, float dpi);
void ui_set_colors(Color text, Color bg, Color surface, Color circle, Color button, Color button_hover, Color icon);
int ui_is_desktop_mode(void);
void ui_set_frame(Camera2D camera);
void flint_ui_set_text_input_platform_callback(FlintUITextInputPlatformCallback callback);
void ui_set_cursor_clickable(int *cursor_clickable);
void ui_set_cursor_disabled(int *cursor_disabled);
void ui_set_icons(Texture2D gear_icon, Texture2D x_icon);
void ui_set_input_blocked(int blocked);
int ui_input_captures_click(Vector2 point);
int ui_hover_effects_enabled(void);
/* DPI scaling, color, and layout functions now from Flint: flint_px, flint_clamp_px, flint_lighten, flint_darken, flint_centered_column, flint_page_side_padding */
int flint_ui_font(void);
int flint_ui_font_small(void);
int flint_ui_title_font(const char *title, int max_width);
int flint_ui_text_y(const char *text, int box_y, int box_h, int font);
void flint_ui_draw_text_centered(const char *text, int center_x, int center_y, int font, Color color);
void flint_ui_draw_text_left_in_rect(const char *text, Rectangle rect, int font_size, Color color);
void ui_draw_fitted_text_in_rect(const char *text, Rectangle rect, int preferred_size, int min_size, Color color);
void flint_ui_draw_text_input(Rectangle bounds, const char *text, int cursor_position,
                              int focused, int cursor_visible, int font,
                              FlintUITextInputStyle style);
int flint_ui_text_edit(FlintUITextEdit edit);
void flint_ui_text_input_queue_codepoint(int codepoint);
void flint_ui_text_input_queue_backspace(void);
void flint_ui_text_input_queue_enter(void);
int flint_ui_button(FlintUIButton button);
int flint_ui_icon_button(FlintUIIconButton button);
int flint_ui_text_input(FlintUITextInput input);
int flint_ui_text_field(FlintUITextField field);
int flint_ui_paragraph_height(FlintUIParagraph paragraph);
void flint_ui_paragraph_draw(FlintUIParagraph paragraph, int x, int *y);
FlintUIGuideResult flint_ui_draw_guide_overlay(FlintUIGuideOverlay guide);
void ui_draw_bevel(int x, int y, int w, int h, Color light, Color dark);
void ui_draw_text_lines(const char **lines, int count, int x, int *y, int font, int line_h, Color color);
/* Icon fallback drawing now from Flint: flint_draw_icon_fallback */
int ui_icon_btn_size(UIIconSize size);
int ui_icon_btn_padding(UIIconSize size);
int ui_draw_icon_btn(int x, int y, UIIconSize size, Texture2D icon, int *hover);
int ui_draw_icon_btn_padded(int x, int y, int size, int padding, Texture2D icon, int *hover);
int ui_draw_info_button(int center_x, int center_y, int diameter);
int ui_draw_icon_slider_popup(FlintUIIconSliderPopup popup);
FlintUIIconRowResult ui_draw_bottom_icon_row(FlintUIBottomIconRow row);
int ui_bottom_nav_height(void);
FlintUIBottomNavResult ui_draw_bottom_nav(FlintUIBottomNav nav);
FlintUIBottomNavConfigResult ui_draw_bottom_nav_config_modal(FlintUIBottomNavConfigModal modal);
FlintUIToolbarResult ui_draw_toolbar(FlintUIToolbar toolbar);
FlintUIToolbarHeaderResult ui_draw_toolbar_header(FlintUIToolbarHeader header);
void ui_draw_info_rows(FlintUIInfoRows rows);
int ui_label_text_field_height(FlintUILabelTextField row);
int ui_draw_label_text_field(FlintUILabelTextField row, int x, int y, int w);
int ui_section_label_height(FlintUISectionLabel label);
int ui_draw_section_label(FlintUISectionLabel label, int x, int y);
int ui_checkbox_row_height(FlintUICheckboxRow row);
int ui_draw_checkbox_row(FlintUICheckboxRow row, int x, int y);
int ui_button_row_height(FlintUIButtonRow row);
int ui_draw_button_row(FlintUIButtonRow row);
int ui_draw_text_btn(int x, int y, const char *label, int *hover);

/* Generic button component with unified styling */
int ui_draw_generic_button(int x, int y, int w, int h, const char *label,
                           UIButtonStyle style, int disabled, int *hover);
int ui_draw_subtab_bar(FlintUISubtabBar bar);
int ui_draw_tab_bar(FlintUITabBar bar);
int ui_tab_bar_height(void);

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
int ui_draw_checkbox_toggle_disabled(int x, int y, const char *label,
                                     int *value, int disabled);
int ui_draw_dropdown_button(int id, int x, int y, int w, int h, const char **options, int option_count, int *selected_index);
int ui_draw_dropdown_menu(int id);
int ui_dropdown_captures_click(Vector2 point);
void ui_set_dropdown_clip_top(int top);
void ui_set_dropdown_clip_bottom(int bottom);
int ui_draw_theme_switcher(int x, int y, int w, const char *label,
                           const char *light_label, const char *dark_label,
                           int *theme_id, int *dark_mode);
int ui_draw_theme_picker(int x, int y, int w, int dark_mode, int *theme_id);
int ui_theme_picker_height(int w);
void ui_draw_tutorial_image_placeholder(const char *label, int x, int y, int w, int h);
void ui_draw_tutorial_image(Texture2D texture, const char *fallback, int x, int y, int w, int h);
int ui_draw_modal(const char *title, const char *message, const char *cancel_btn, const char *confirm_btn);
int ui_draw_modal_3btn(const char *title, const char *message, const char *left_btn, const char *middle_btn, const char *right_btn);
int ui_paragraph_modal_height(FlintUIParagraphModalMeasure measure);
int ui_draw_screen_header(const char *title, int show_close);
int ui_screen_header_height(void);
FlintUIHeader ui_draw_title_header(int height, const char *title,
                                   Texture2D left_icon,
                                   Texture2D right_icon);
FlintUIPanelFrame ui_draw_modal_frame(int width, int height, const char *title,
                                      Texture2D left_icon,
                                      Texture2D right_icon);
int ui_scrollbar_reserved_width(int max_scroll);
int ui_scrollbar_content_width(int content_width, int max_scroll);
int ui_scrollbar_safe_content_width(int content_x, int content_width,
                                    int scrollbar_x, int max_scroll);
FlintUIScrollView ui_scroll_container_measure(FlintUIScrollArea area);
FlintUIScrollView ui_scroll_container_begin(FlintUIScrollArea area);
void ui_scroll_container_end(FlintUIScrollArea area, FlintUIScrollView view);
FlintUIScrollPage ui_scroll_page_begin(FlintUIScrollPageSpec spec);
void ui_scroll_page_end(FlintUIScrollPage page);
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
