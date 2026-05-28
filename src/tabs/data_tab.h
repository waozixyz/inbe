#ifndef DATA_TAB_H
#define DATA_TAB_H

typedef struct InbeApp InbeApp;

/* Draw the data management tab */
void data_tab_draw(InbeApp *app);

/* Handle tab click - switch to data screen */
void data_tab_on_click(void *user_data);

#endif
