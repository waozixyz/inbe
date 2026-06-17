#ifndef SETTINGS_TAB_H
#define SETTINGS_TAB_H

#include "breath_engine.h"
#include "app_fwd.h"

/* Draw the settings screen */
void settings_screen_draw(InbeApp *app);
void practice_config_screen_draw(InbeApp *app);
void settings_screen_clear_status(void);
void settings_screen_set_status_success(const char *message, const char *detail);
void settings_screen_set_status_error(const char *message);

#endif
