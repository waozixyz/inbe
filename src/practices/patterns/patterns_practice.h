#ifndef APP_PATTERNS_PRACTICE_H
#define APP_PATTERNS_PRACTICE_H

#include "app_fwd.h"

/* Timed-ratio breathing: inhale / hold-in / exhale / hold-out cycles. */
int patterns_preset_count(void);
int patterns_preset_ratio(int preset, int part);
const char *patterns_preset_label_key(int preset);
void patterns_practice_init(InnerBreeze*app);
void patterns_practice_destroy(InnerBreeze*app);
void patterns_practice_start(InnerBreeze*app);
void patterns_manual_draw(InnerBreeze*app);
void patterns_manual_close(InnerBreeze*app, int mark_seen);
void patterns_config_screen_draw(InnerBreeze*app);
void patterns_practice_leave_config(InnerBreeze*app);
void patterns_draw_screen(InnerBreeze*app, int center_x, int center_y);
void patterns_request_exit(InnerBreeze*app);
void patterns_advance_elapsed(InnerBreeze*app, int elapsed_ms);
void patterns_ratio_text(const InnerBreeze*app, char *out, size_t out_size);
const char *patterns_phase_label_key(int phase);
int patterns_phase_remaining_seconds(const InnerBreeze*app);

#endif
