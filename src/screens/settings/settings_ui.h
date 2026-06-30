#ifndef INBE_SETTINGS_UI_H
#define INBE_SETTINGS_UI_H

int settings_ui_toggle_row_height(const char *label, int w);
int settings_ui_draw_toggle_row(int x, int w, int *y, const char *label, int *value);

#endif
