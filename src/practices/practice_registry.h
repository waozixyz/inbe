#ifndef INBE_PRACTICE_REGISTRY_H
#define INBE_PRACTICE_REGISTRY_H

#include "app_fwd.h"

typedef enum PracticeId {
    PRACTICE_WHM = 0,
    PRACTICE_MEDITATION = 1,
    PRACTICE_COUNT = 2
} PracticeId;

typedef struct PracticeDefinition {
    PracticeId id;
    const char *label_key;
    void (*init)(InbeApp *app);
    void (*destroy)(InbeApp *app);
    void (*start)(InbeApp *app);
    void (*draw_manual)(InbeApp *app);
    void (*close_manual)(InbeApp *app, int mark_seen);
    void (*draw_config)(InbeApp *app);
    void (*leave_config)(InbeApp *app);
    void (*update)(InbeApp *app);
    void (*draw_setup_modal)(InbeApp *app);
    void (*draw_active_session)(InbeApp *app, int center_x, int center_y);
    void (*request_exit)(InbeApp *app);
    int (*is_active)(const InbeApp *app);
    void (*background_start)(InbeApp *app);
    void (*background_stop)(InbeApp *app);
    void (*advance_elapsed)(InbeApp *app, int elapsed_ms);
} PracticeDefinition;

const PracticeDefinition *practice_get(int id);
int practice_count(void);
int practice_clamp_id(int id);
const char *practice_label(int id);
void practice_update_circle_bounds(InbeApp *app, int top_reserve, int bottom_reserve);
void practice_update_session_sounds(InbeApp *app);
int practice_ensure_results_saved(InbeApp *app);
int practice_draw_start_preview(InbeApp *app, int center_x, int center_y);
void practice_draw_active_breathing(InbeApp *app, int center_x, int center_y);
void practice_update_active_breathing(InbeApp *app, int center_x, int center_y, int *hover);
void practice_draw_results(InbeApp *app, int center_x, int center_y, int *hover);
const PracticeDefinition *practice_active(const InbeApp *app);
int practice_active_supports_background(const InbeApp *app);
void practice_active_background_start(InbeApp *app);
void practice_active_background_stop(InbeApp *app);
void practice_active_advance_elapsed(InbeApp *app, int elapsed_ms);

#endif
