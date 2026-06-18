#ifndef INBE_APP_SETTINGS_H
#define INBE_APP_SETTINGS_H

#include "app_fwd.h"
#include "breath_engine.h"

void save_settings(InbeApp *app);
int app_load_settings(InbeApp *app);
void reset_settings_preview(InbeApp *app);
void apply_settings(Inbe *inbe, int speed, int max_rounds, int max_breaths, int pause_seconds);

#endif
