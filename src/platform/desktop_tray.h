#ifndef INBE_DESKTOP_TRAY_H
#define INBE_DESKTOP_TRAY_H

#include "app_fwd.h"

typedef enum InbeDesktopTrayAction {
    INBE_DESKTOP_TRAY_ACTION_NONE = 0,
    INBE_DESKTOP_TRAY_ACTION_SHOW,
    INBE_DESKTOP_TRAY_ACTION_MINIMIZE,
    INBE_DESKTOP_TRAY_ACTION_CLOSE_REQUEST,
    INBE_DESKTOP_TRAY_ACTION_START_WHM,
    INBE_DESKTOP_TRAY_ACTION_START_MEDITATION,
    INBE_DESKTOP_TRAY_ACTION_START_SUN_SALUTATION,
    INBE_DESKTOP_TRAY_ACTION_QUIT
} InbeDesktopTrayAction;

int inbe_desktop_tray_init(void);
void inbe_desktop_tray_shutdown(void);
InbeDesktopTrayAction inbe_desktop_tray_poll_action(void);
void inbe_desktop_tray_apply_action(InbeApp *app, InbeDesktopTrayAction action, int *quit);
void inbe_desktop_tray_update_status(InbeApp *app);
void inbe_desktop_tray_keep_running(void);

#endif
