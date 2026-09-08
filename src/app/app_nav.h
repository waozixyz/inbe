#ifndef APP_APP_NAV_H
#define APP_APP_NAV_H

#include "app_fwd.h"
#include <stddef.h>

extern int view_width;
extern int view_height;

int app_current_nav_route(const InnerBreeze*app);
int app_nav_sidebar_screen_active(const InnerBreeze*app);
int app_screen_for_main_tab(int main_tab);
const char *app_nav_route_label(int route);
int app_nav_desktop_rail_width(void);
int app_nav_desktop_content_offset(void);
int app_nav_desktop_rail_enabled(const InnerBreeze*app);
void app_nav_profile_identity(InnerBreeze*app, char *out, size_t out_size,
                              char *subtitle, size_t subtitle_size);
int app_active_practice_title_height(void);
int app_draw_active_practice_title_bar(InnerBreeze*app, const char *title,
                                       int height);
int app_page_height(const InnerBreeze*app, int full_height);
int app_content_left_reserved(const InnerBreeze*app);
int app_content_bottom_reserved(const InnerBreeze*app);
int app_fullscreen_bottom_reserved(const InnerBreeze*app);
void app_draw_bottom_nav(InnerBreeze*app);
void app_draw_nav_sidebar(InnerBreeze*app);
int app_draw_customize_nav_page(InnerBreeze*app);
void app_open_customize_nav(InnerBreeze*app);
void app_apply_nav_route(InnerBreeze*app, int route);
void app_update_nav_sidebar_mode(InnerBreeze*app);
void app_close_nav_sidebar(InnerBreeze*app);
int app_return_to_nav_sidebar_if_needed(InnerBreeze*app);
void app_reset_bottom_nav_routes(InnerBreeze*app);
void app_sanitize_bottom_nav_routes(InnerBreeze*app);

#endif
