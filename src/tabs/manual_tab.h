#ifndef MANUAL_TAB_H
#define MANUAL_TAB_H

#include "../libinbe/inbe.h"

typedef struct InbeApp InbeApp;

/* Draw the manual/tutorial screen */
void manual_tab_draw(InbeApp *app);

/* Close the tutorial, optionally marking it as seen */
void manual_tab_close_tutorial(InbeApp *app, int mark_seen);

#endif
