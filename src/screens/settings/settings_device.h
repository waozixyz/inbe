#ifndef INBE_SETTINGS_DEVICE_H
#define INBE_SETTINGS_DEVICE_H

#include "app_fwd.h"

typedef struct SettingsDeviceState {
    int draw_language_menu;
    int language_menu_changed;
    int draw_orientation_menu;
    int orientation_changed;
} SettingsDeviceState;

int settings_device_content_height(void);
void settings_device_draw(InbeApp *app, int x, int w, int *y, SettingsDeviceState *state);
void settings_device_handle_overlays(InbeApp *app, SettingsDeviceState *state);

#endif
