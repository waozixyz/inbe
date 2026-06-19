#ifndef INBE_SETTINGS_SYNC_ACCOUNT_H
#define INBE_SETTINGS_SYNC_ACCOUNT_H

#include "app_fwd.h"

typedef int (*SettingsSyncKeySaveDialog)(InbeApp *app, const char *filename);

enum {
    SETTINGS_SYNC_ACCOUNT_HEIGHT = 132
};

void settings_sync_account_set_save_dialog(SettingsSyncKeySaveDialog callback);
void settings_sync_account_draw(InbeApp *app, int x, int w, int *y);
int settings_sync_account_draw_backup_modal(InbeApp *app);

#endif
