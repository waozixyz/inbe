#ifndef PRACTICE_SCREEN_H
#define PRACTICE_SCREEN_H

#include "raylib.h"

#define PRACTICE_CATEGORY_TAB_H 40
#define PRACTICE_CATEGORY_TOAST_TICKS 120
#define PRACTICE_CONFIG_TAB_LIST (-1)

/* Forward declarations */
typedef struct InbeApp InbeApp;

/* Global arrays */
extern const char *g_practice_category_labels[3];
extern const int g_practice_category_default_themes[3];

void practice_ensure_enabled_selection(InbeApp *app);
int practice_enabled_count(InbeApp *app);
int practice_active_theme(InbeApp *app);
void practice_sync_global_theme(InbeApp *app);
int practice_activity_count_for_tab(int tab);
int practice_activity_for_tab(int tab, int index);
const char *practice_activity_label(int exercise);
int practice_activity_index_for_tab(int tab, int exercise);
void practice_clamp_activity_to_tab(InbeApp *app);
Color practice_theme_color(InbeApp *app, int tab_index);
int practice_category_bottom_y_for_app(InbeApp *app);

void draw_practice_coming_soon_popout(InbeApp *app);
void draw_practice_category_tabs(InbeApp *app);
int draw_theme_circle_button(InbeApp *app, int x, int y, int radius, int theme_id);
void draw_practice_config_button(InbeApp *app);
void draw_practice_config_page(InbeApp *app);

void on_practice_tab_click(void *user_data);
void habits_sync_topic_theme_colors(InbeApp *app, int sync_topic, int save_now);
void app_request_bottom_tab(InbeApp *app, int bottom_tab);

#endif
