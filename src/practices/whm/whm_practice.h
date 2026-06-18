#ifndef INBE_WHM_PRACTICE_H
#define INBE_WHM_PRACTICE_H

#include "app_fwd.h"

void whm_practice_init(InbeApp *app);
void whm_practice_destroy(InbeApp *app);
void whm_practice_start(InbeApp *app);
void whm_practice_leave_config(InbeApp *app);
void whm_manual_draw(InbeApp *app);
void whm_manual_close(InbeApp *app, int mark_seen);
void whm_config_screen_draw(InbeApp *app);

#endif
