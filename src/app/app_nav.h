#ifndef INBE_APP_NAV_H
#define INBE_APP_NAV_H

#include "app_fwd.h"

int app_current_nav_route(const InbeApp *app);
int app_page_height(const InbeApp *app, int full_height);
int app_content_bottom_reserved(const InbeApp *app);
int app_fullscreen_bottom_reserved(const InbeApp *app);
void app_set_android_bottom_nav_height(int height);
int app_android_bottom_nav_height(void);
void app_draw_bottom_nav(InbeApp *app);
int app_draw_customize_nav_page(InbeApp *app);
void app_apply_nav_route(InbeApp *app, int route);
void app_reset_bottom_nav_routes(InbeApp *app);
void app_sanitize_bottom_nav_routes(InbeApp *app);

#endif
