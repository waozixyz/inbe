#ifndef INBE_PROFILE_SOCIAL_H
#define INBE_PROFILE_SOCIAL_H

#include "app_fwd.h"

void profile_social_load_friends_cache(InbeApp *app);
void profile_social_load_leaderboard_cache(InbeApp *app);
int profile_social_friends_count(InbeApp *app);
int profile_social_pending_count(InbeApp *app);
void profile_social_draw_friends(InbeApp *app, int x, int w, int *y);
void profile_social_draw_leaderboard(InbeApp *app, int x, int w, int *y);

#endif
