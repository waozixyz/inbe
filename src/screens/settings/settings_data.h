#ifndef INBE_SETTINGS_DATA_H
#define INBE_SETTINGS_DATA_H

#include "app_fwd.h"

int settings_data_content_height(int content_w);
void settings_data_draw_actions(InbeApp *app, int x, int w, int *y);
void settings_data_draw_sync_status(int x, int w, int *y);
void settings_data_open_sync_account_config(InbeApp *app);
int settings_data_file_dialog_active(void);
int settings_data_draw_pending_file_dialog(InbeApp *app);
int settings_data_draw_modals(InbeApp *app);
void settings_data_handle_android_import(InbeApp *app);
void settings_data_handle_web_import(InbeApp *app);

#endif
