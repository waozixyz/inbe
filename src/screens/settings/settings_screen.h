#ifndef SETTINGS_TAB_H
#define SETTINGS_TAB_H

#include "breath_engine.h"
#include "app_fwd.h"

/* Draw the settings screen. Returns non-zero when it consumed the whole frame. */
int settings_screen_draw(InbeApp *app);
const char *settings_screen_tab_label(int tab);
void settings_screen_clear_status(void);
void settings_screen_set_status_success(const char *message, const char *detail);
void settings_screen_set_status_error(const char *message);
void settings_screen_draw_status_reserved(int x, int *y, int reserved_h);

#endif
