#ifndef PRACTICE_SCREEN_H
#define PRACTICE_SCREEN_H

#include "raylib.h"
#include "app_fwd.h"

#define PRACTICE_CATEGORY_TAB_H 40
#define PRACTICE_CATEGORY_TOAST_TICKS 120
#define PRACTICE_CONFIG_TAB_LIST (-1)

int practice_activity_count_for_tab(int tab);
int practice_activity_for_tab(int tab, int index);
const char *practice_activity_label(int exercise);
int practice_activity_index_for_tab(int tab, int exercise);
void practice_clamp_activity_to_tab(InbeApp *app);
void practice_screen_open_tab(InbeApp *app, int tab);
void practice_screen_draw_top_bar(InbeApp *app, int draw_menu);
void practice_screen_prepare_first_run_guide(InbeApp *app);
void practice_screen_draw_first_run_guide(InbeApp *app);

#endif
