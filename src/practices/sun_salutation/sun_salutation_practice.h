#ifndef INBE_SUN_SALUTATION_PRACTICE_H
#define INBE_SUN_SALUTATION_PRACTICE_H

#include "app_fwd.h"

#define SUN_SALUTATION_POSE_COUNT 8
#define SUN_SALUTATION_STEP_COUNT 12
#define SUN_SALUTATION_SECONDS_MIN 3
#define SUN_SALUTATION_SECONDS_MAX 12
#define SUN_SALUTATION_DEFAULT_REPETITIONS 3
#define SUN_SALUTATION_DEFAULT_START_SECONDS 8
#define SUN_SALUTATION_DEFAULT_END_SECONDS 5

int sun_salutation_step_pose_index(int step);
const char *sun_salutation_step_label(int step);
void sun_salutation_practice_init(InbeApp *app);
void sun_salutation_practice_destroy(InbeApp *app);
void sun_salutation_practice_start(InbeApp *app);
void sun_salutation_config_screen_draw(InbeApp *app);
void sun_salutation_draw_screen(InbeApp *app, int center_x, int center_y);
void sun_salutation_request_exit(InbeApp *app);

#endif
