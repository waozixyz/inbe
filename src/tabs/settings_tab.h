#ifndef SETTINGS_TAB_H
#define SETTINGS_TAB_H

#include "../libinbe/inbe.h"

typedef struct InbeApp InbeApp;

/* Draw the settings screen */
void settings_tab_draw(InbeApp *app);
void settings_tab_clear_status(void);

#endif
