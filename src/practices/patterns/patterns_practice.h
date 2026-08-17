#ifndef INBE_PATTERNS_PRACTICE_H
#define INBE_PATTERNS_PRACTICE_H

#include "app_fwd.h"

/* Timed-ratio breathing: inhale / hold-in / exhale / hold-out cycles. */
int patterns_preset_count(void);
void patterns_practice_init(InbeApp *app);
void patterns_practice_destroy(InbeApp *app);
void patterns_practice_start(InbeApp *app);
void patterns_config_screen_draw(InbeApp *app);
void patterns_practice_leave_config(InbeApp *app);
void patterns_draw_screen(InbeApp *app, int center_x, int center_y);
void patterns_request_exit(InbeApp *app);
void patterns_ratio_text(const InbeApp *app, char *out, size_t out_size);

#endif
