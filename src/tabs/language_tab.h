#ifndef LANGUAGE_TAB_H
#define LANGUAGE_TAB_H

#include "../libinbe/inbe.h"
#include "../app_fwd.h"

void language_tab_draw(InbeApp *app);
int language_dropdown_button(InbeApp *app, int id, int x, int y, int w, int h, int *selected_index);
int language_dropdown_menu(InbeApp *app, int id);

#endif
