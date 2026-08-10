#ifndef INBE_WHM_SESSION_H
#define INBE_WHM_SESSION_H

#include "app.h"

void session_reset_round_breathe(Inbe *inbe);
void session_update_circle_bounds_for_view(Inbe *inbe, int top_reserve, int bottom_reserve);
void session_start(InbeApp *app);
int session_ensure_results_saved(InbeApp *app);
void session_discard_saved_results(InbeApp *app);
int session_has_completed_rounds(InbeApp *app);
void session_draw_inbe(InbeApp *app, int center_x, int center_y);
void draw_session_progress_circle(InbeApp *app, int center_x, int center_y, float radius);
int session_draw_start_preview(InbeApp *app, int center_x, int center_y);
void session_update_screen(InbeApp *app, int center_x, int center_y, int *hover);
void session_draw_results_screen(InbeApp *app, int center_x, int center_y, int *hover);
void update_session_sounds(InbeApp *app);
void session_background_start(InbeApp *app);
void session_advance_elapsed(InbeApp *app, int elapsed_ms);
void update_preview_bounds(Inbe *inbe, int content_w, int max_h);
void draw_preview_inbe(Inbe *inbe, int center_x, int center_y);

#endif
