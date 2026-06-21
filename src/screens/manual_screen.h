#ifndef MANUAL_TAB_H
#define MANUAL_TAB_H

#include "breath_engine.h"
#include "app_fwd.h"

typedef void (*ManualGuideStartFn)(InbeApp *app);
typedef void (*ManualGuideCloseFn)(InbeApp *app, int mark_seen);

typedef struct ManualGuideNav {
    int page;
    int page_count;
    int y;
    int h;
    int show_left_on_first;
    ManualGuideStartFn start;
    ManualGuideCloseFn close;
} ManualGuideNav;

void manual_screen_draw(InbeApp *app);
void manual_screen_close_tutorial(InbeApp *app, int mark_seen);
void manual_screen_reset_layouts(InbeApp *app);
int manual_screen_guide_update_page(InbeApp *app, int page_count,
                                    ManualGuideStartFn start,
                                    ManualGuideCloseFn close);
void manual_screen_guide_draw_nav(InbeApp *app, ManualGuideNav nav);
int manual_screen_guide_nav_height(void);

#endif
