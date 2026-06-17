#ifndef PRACTICE_SCREEN_H
#define PRACTICE_SCREEN_H

#include "raylib.h"

#define PRACTICE_CATEGORY_TAB_H 40
#define PRACTICE_CATEGORY_TOAST_TICKS 120
#define PRACTICE_CONFIG_TAB_LIST (-1)

/* Forward declarations */
typedef struct InbeApp InbeApp;

int practice_activity_count_for_tab(int tab);
int practice_activity_for_tab(int tab, int index);
const char *practice_activity_label(int exercise);
int practice_activity_index_for_tab(int tab, int exercise);
void practice_clamp_activity_to_tab(InbeApp *app);

void on_practice_tab_click(void *user_data);
void app_request_bottom_tab(InbeApp *app, int bottom_tab);

#endif
