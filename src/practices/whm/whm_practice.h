#ifndef APP_WHM_PRACTICE_H
#define APP_WHM_PRACTICE_H

#include "app_fwd.h"

void whm_practice_init(InnerBreeze*app);
void whm_practice_destroy(InnerBreeze*app);
void whm_practice_start(InnerBreeze*app);
void whm_practice_leave_config(InnerBreeze*app);
void whm_manual_draw(InnerBreeze*app);
void whm_manual_close(InnerBreeze*app, int mark_seen);
void whm_config_screen_draw(InnerBreeze*app);

#endif
