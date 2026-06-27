#ifndef INBE_SETTINGS_SESSION_H
#define INBE_SETTINGS_SESSION_H

#include "app_fwd.h"

int settings_session_content_height(int content_w);
void settings_session_draw(InbeApp *app, int x, int w, int *y);

#endif
