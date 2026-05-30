#ifndef MANUAL_TAB_H
#define MANUAL_TAB_H

#include "../libinbe/inbe.h"

typedef struct InbeApp InbeApp;

void manual_tab_draw(InbeApp *app);
void manual_tab_close_tutorial(InbeApp *app, int mark_seen);

#endif
