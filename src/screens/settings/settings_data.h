#ifndef INBE_SETTINGS_DATA_H
#define INBE_SETTINGS_DATA_H

#include "app_fwd.h"

int settings_data_content_height(int content_w);
void settings_data_draw(InbeApp *app, int x, int w, int *y);
int settings_data_draw_pending_file_dialog(InbeApp *app);
int settings_data_draw_modals(InbeApp *app);
void settings_data_handle_android_import(InbeApp *app);
void settings_data_handle_web_import(InbeApp *app);

#endif
