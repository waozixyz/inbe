#ifndef MANUAL_TAB_H
#define MANUAL_TAB_H

#include "breath_engine.h"
#include "app_fwd.h"

void manual_screen_draw(InbeApp *app);
void manual_screen_close_tutorial(InbeApp *app, int mark_seen);
void manual_screen_reset_layouts(InbeApp *app);

#endif
