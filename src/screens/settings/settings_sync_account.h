#ifndef INBE_SETTINGS_SYNC_ACCOUNT_H
#define INBE_SETTINGS_SYNC_ACCOUNT_H

#include "app_fwd.h"
#include "storage/sync_account.h"

typedef int (*SettingsSyncKeySaveDialog)(InbeApp *app, const char *filename);
typedef int (*SettingsSyncKeyImportDialog)(InbeApp *app);

void settings_sync_account_set_save_dialog(SettingsSyncKeySaveDialog callback);
void settings_sync_account_set_import_dialog(SettingsSyncKeyImportDialog callback);
void settings_sync_account_draw(InbeApp *app, int x, int w, int *y);
int settings_sync_account_config_content_height(int content_w);
void settings_sync_account_draw_config(InbeApp *app, int x, int w, int *y);
void settings_sync_account_clear_remote_confirmed(InbeApp *app);
int settings_sync_account_save_prepared(InbeApp *app, InbeSyncAccount *account,
                                        int action, int clear_local_data);
int settings_sync_account_draw_backup_modal(InbeApp *app);
int settings_sync_account_draw_alias_modal(InbeApp *app);
int settings_sync_account_draw_public_id_modal(InbeApp *app);

#endif
