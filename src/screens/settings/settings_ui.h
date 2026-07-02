#ifndef INBE_SETTINGS_UI_H
#define INBE_SETTINGS_UI_H

#include "app_fwd.h"

int settings_ui_toggle_row_height(const char *label, int w);
int settings_ui_draw_toggle_row(int x, int w, int *y, const char *label, int *value);
int settings_ui_commit_toggle_row(InbeApp *app, int x, int w, int *y,
                                  const char *label, int *value, int save_now);
int settings_ui_commit_slider_row(InbeApp *app, int id, int x, int w, int *y,
                                  int row_h, const char *label, int min, int max,
                                  int *value, const char *suffix, int save_now);

#endif
