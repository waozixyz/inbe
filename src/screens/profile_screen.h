#ifndef INBE_PROFILE_SCREEN_H
#define INBE_PROFILE_SCREEN_H

#include "app_fwd.h"

int profile_screen_draw(InbeApp *app);
void profile_screen_refresh_social_cache(InbeApp *app);
int profile_screen_first_run_guide_active(const InbeApp *app);
void profile_screen_prepare_first_run_guide(InbeApp *app);
void profile_screen_draw_first_run_guide(InbeApp *app);

#endif
