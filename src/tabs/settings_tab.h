#ifndef SETTINGS_TAB_H
#define SETTINGS_TAB_H

#include "../libinbe/inbe.h"

typedef struct InbeApp InbeApp;

/* Draw the settings screen */
void settings_tab_draw(InbeApp *app);
void settings_tab_clear_status(void);
void settings_tab_set_status_success(const char *message, const char *detail);
void settings_tab_set_status_error(const char *message);

#endif
