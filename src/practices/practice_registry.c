#include "practice_registry.h"

#include "app.h"
#include "locale.h"
#include "practices/whm/whm_practice.h"
#include "practices/whm/whm_session.h"
#include "practices/meditation/meditation_practice.h"

static const PracticeDefinition g_practices[PRACTICE_COUNT] = {
    {
        .id = PRACTICE_WHM,
        .label_key = "exercise_wim_hof",
        .init = whm_practice_init,
        .destroy = whm_practice_destroy,
        .start = whm_practice_start,
        .draw_manual = whm_manual_draw,
        .close_manual = whm_manual_close,
        .draw_config = whm_config_screen_draw,
        .leave_config = whm_practice_leave_config,
        .update = NULL,
        .draw_setup_modal = NULL,
        .draw_active_session = NULL,
        .request_exit = NULL,
    },
    {
        .id = PRACTICE_MEDITATION,
        .label_key = "exercise_meditation",
        .init = meditation_practice_init,
        .destroy = meditation_practice_destroy,
        .start = meditation_practice_start,
        .draw_manual = meditation_manual_draw,
        .close_manual = meditation_manual_close,
        .draw_config = meditation_config_screen_draw,
        .leave_config = meditation_practice_leave_config,
        .update = meditation_practice_update,
        .draw_setup_modal = meditation_draw_setup_modal,
        .draw_active_session = meditation_draw_screen,
        .request_exit = meditation_request_exit,
    },
};

const PracticeDefinition *
practice_get(int id)
{
    id = practice_clamp_id(id);
    return &g_practices[id];
}

int
practice_count(void)
{
    return PRACTICE_COUNT;
}

int
practice_clamp_id(int id)
{
    if(id < 0 || id >= PRACTICE_COUNT)
        return PRACTICE_WHM;
    return id;
}

const char *
practice_label(int id)
{
    return locale_get(practice_get(id)->label_key);
}

void
practice_update_circle_bounds(InbeApp *app, int top_reserve, int bottom_reserve)
{
    if(app != NULL)
        session_update_circle_bounds_for_view(&app->inbe, top_reserve, bottom_reserve);
}

void
practice_update_session_sounds(InbeApp *app)
{
    update_session_sounds(app);
}

int
practice_ensure_results_saved(InbeApp *app)
{
    return session_ensure_results_saved(app);
}

int
practice_draw_start_preview(InbeApp *app, int center_x, int center_y)
{
    return session_draw_start_preview(app, center_x, center_y);
}

void
practice_draw_active_breathing(InbeApp *app, int center_x, int center_y)
{
    session_draw_inbe(app, center_x, center_y);
}

void
practice_update_active_breathing(InbeApp *app, int center_x, int center_y, int *hover)
{
    session_update_screen(app, center_x, center_y, hover);
}

void
practice_draw_results(InbeApp *app, int center_x, int center_y, int *hover)
{
    session_draw_results_screen(app, center_x, center_y, hover);
}
