#ifndef INBE_MEDITATION_PRACTICE_H
#define INBE_MEDITATION_PRACTICE_H

#include "app_fwd.h"

void meditation_practice_init(InbeApp *app);
void meditation_practice_destroy(InbeApp *app);
void meditation_practice_start(InbeApp *app);
void meditation_practice_update(InbeApp *app);
void meditation_practice_leave_config(InbeApp *app);
void meditation_manual_draw(InbeApp *app);
void meditation_manual_close(InbeApp *app, int mark_seen);
void meditation_config_screen_draw(InbeApp *app);
void meditation_draw_setup_modal(InbeApp *app);
void meditation_draw_screen(InbeApp *app, int center_x, int center_y);
void meditation_request_exit(InbeApp *app);

#endif
