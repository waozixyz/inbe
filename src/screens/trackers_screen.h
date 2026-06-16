#ifndef TRACKERS_SCREEN_H
#define TRACKERS_SCREEN_H

#include "raylib.h"

#define TRACKER_CATEGORY_TAB_H 40
#define TRACKER_CATEGORY_TOAST_TICKS 120
#define TRACKER_CONFIG_TAB_LIST (-1)

/* Forward declarations */
typedef struct InbeApp InbeApp;

/* Global arrays */
extern const char *g_tracker_category_labels[3];
extern const int g_tracker_category_default_themes[3];

void tracker_ensure_enabled_selection(InbeApp *app);
int tracker_enabled_count(InbeApp *app);
int tracker_active_theme(InbeApp *app);
void tracker_sync_global_theme(InbeApp *app);
int tracker_activity_count_for_tab(int tab);
int tracker_activity_for_tab(int tab, int index);
const char *tracker_activity_label(int exercise);
int tracker_activity_index_for_tab(int tab, int exercise);
void tracker_clamp_activity_to_tab(InbeApp *app);
Color tracker_theme_color(InbeApp *app, int tab_index);
int tracker_category_bottom_y_for_app(InbeApp *app);

void draw_tracker_coming_soon_popout(InbeApp *app);
void draw_tracker_category_tabs(InbeApp *app);
int draw_theme_circle_button(InbeApp *app, int x, int y, int radius, int theme_id);
void draw_tracker_config_button(InbeApp *app);
void draw_tracker_config_page(InbeApp *app);

void on_tracker_tab_click(void *user_data);
void habits_sync_topic_theme_colors(InbeApp *app, int sync_topic, int save_now);

#endif
