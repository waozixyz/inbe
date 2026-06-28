#ifndef INBE_APP_NAV_H
#define INBE_APP_NAV_H

#include "app_fwd.h"

int app_current_nav_route(const InbeApp *app);
int app_content_bottom_reserved(const InbeApp *app);
void app_draw_bottom_nav(InbeApp *app);
void app_apply_nav_route(InbeApp *app, int route);

#endif
