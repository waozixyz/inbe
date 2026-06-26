#ifndef INBE_SUN_SALUTATION_PRACTICE_H
#define INBE_SUN_SALUTATION_PRACTICE_H

#include "app_fwd.h"

void sun_salutation_practice_init(InbeApp *app);
void sun_salutation_practice_destroy(InbeApp *app);
void sun_salutation_practice_start(InbeApp *app);
void sun_salutation_config_screen_draw(InbeApp *app);
void sun_salutation_draw_screen(InbeApp *app, int center_x, int center_y);
void sun_salutation_request_exit(InbeApp *app);

#endif
