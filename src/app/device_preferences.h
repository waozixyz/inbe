#ifndef INBE_APP_PREFERENCES_H
#define INBE_APP_PREFERENCES_H

#include "app.h"

int app_effective_dark_mode(InbeApp *app);
void app_refresh_theme(InbeApp *app);
void app_device_preferences_init(InbeApp *app);
void app_device_preferences_update(InbeApp *app);
void app_apply_orientation_preference(InbeApp *app);

#endif
