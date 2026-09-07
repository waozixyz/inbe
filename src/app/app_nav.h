#ifndef INBE_APP_NAV_H
#define INBE_APP_NAV_H

#include "app_fwd.h"

extern int view_width;
extern int view_height;

int app_current_nav_route(const InbeApp *app);
int app_nav_sidebar_screen_active(const InbeApp *app);
int app_screen_for_main_tab(int main_tab);
const char *app_nav_route_label(int route);
int app_nav_desktop_rail_width(void);
int app_nav_desktop_rail_enabled(const InbeApp *app);
int app_active_practice_title_height(void);
int app_draw_active_practice_title_bar(InbeApp *app, const char *title,
                                       int height);
int app_page_height(const InbeApp *app, int full_height);
int app_content_left_reserved(const InbeApp *app);
int app_content_bottom_reserved(const InbeApp *app);
int app_fullscreen_bottom_reserved(const InbeApp *app);
void app_draw_bottom_nav(InbeApp *app);
void app_draw_nav_sidebar(InbeApp *app);
int app_draw_customize_nav_page(InbeApp *app);
void app_apply_nav_route(InbeApp *app, int route);
void app_update_nav_sidebar_mode(InbeApp *app);
void app_close_nav_sidebar(InbeApp *app);
int app_return_to_nav_sidebar_if_needed(InbeApp *app);
void app_reset_bottom_nav_routes(InbeApp *app);
void app_sanitize_bottom_nav_routes(InbeApp *app);

#endif
