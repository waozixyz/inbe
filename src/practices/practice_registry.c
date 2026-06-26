#include "practice_registry.h"

#include "app.h"
#include "flint_locale.h"
#include "practices/whm/whm_practice.h"
#include "practices/whm/whm_session.h"
#include "practices/meditation/meditation_practice.h"
#include "practices/sun_salutation/sun_salutation_practice.h"

#if ANDROID_BUILD
#include "android_timer.h"
#include "android_wakelock.h"
void set_global_inbe_app(InbeApp *app);
#endif

static int
practice_whm_is_active(const InbeApp *app)
{
    return app != NULL && app->inbe.screen == InbeScreenSession;
}

static int
practice_meditation_is_active(const InbeApp *app)
{
    return app != NULL && app->inbe.screen == InbeScreenMeditation;
}

static int
practice_sun_salutation_is_active(const InbeApp *app)
{
    return app != NULL && app->inbe.screen == InbeScreenSunSalutation;
}

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
        .is_active = practice_whm_is_active,
        .background_start = session_background_start,
        .background_stop = NULL,
        .advance_elapsed = session_advance_elapsed,
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
        .is_active = practice_meditation_is_active,
        .background_start = NULL,
        .background_stop = NULL,
        .advance_elapsed = meditation_advance_elapsed,
    },
    {
        .id = PRACTICE_SUN_SALUTATION,
        .label_key = "exercise_sun_salutation",
        .init = sun_salutation_practice_init,
        .destroy = sun_salutation_practice_destroy,
        .start = sun_salutation_practice_start,
        .draw_manual = NULL,
        .close_manual = NULL,
        .draw_config = sun_salutation_config_screen_draw,
        .leave_config = NULL,
        .update = NULL,
        .draw_setup_modal = NULL,
        .draw_active_session = sun_salutation_draw_screen,
        .request_exit = sun_salutation_request_exit,
        .is_active = practice_sun_salutation_is_active,
        .background_start = NULL,
        .background_stop = NULL,
        .advance_elapsed = NULL,
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

const PracticeDefinition *
practice_active(const InbeApp *app)
{
    for(int i = 0; i < practice_count(); i++) {
        const PracticeDefinition *practice = practice_get(i);
        if(practice->is_active != NULL && practice->is_active(app))
            return practice;
    }
    return NULL;
}

int
practice_active_supports_background(const InbeApp *app)
{
    const PracticeDefinition *practice = practice_active(app);
    return practice != NULL && practice->advance_elapsed != NULL;
}

void
practice_background_start(InbeApp *app, int id)
{
    const PracticeDefinition *practice;

    if(app == NULL)
        return;

    practice = practice_get(id);

#if ANDROID_BUILD
    set_global_inbe_app(app);
#endif

    if(practice == NULL || practice->advance_elapsed == NULL)
        return;

    if(practice->background_start != NULL)
        practice->background_start(app);

#if ANDROID_BUILD
    if(app->inbe.play_in_background) {
        android_wakelock_acquire();
        android_timer_set_app(app);
        android_timer_start();
    }
#endif
}

void
practice_active_background_start(InbeApp *app)
{
    const PracticeDefinition *practice = practice_active(app);
    if(practice == NULL)
        return;
    practice_background_start(app, practice->id);
}

void
practice_active_background_stop(InbeApp *app)
{
    const PracticeDefinition *practice = practice_active(app);

    if(app == NULL)
        return;

    if(practice != NULL && practice->background_stop != NULL)
        practice->background_stop(app);

#if ANDROID_BUILD
    if(app->inbe.play_in_background && practice != NULL &&
       practice->advance_elapsed != NULL) {
        android_wakelock_release();
        android_timer_stop();
    }
#endif
}

void
practice_active_advance_elapsed(InbeApp *app, int elapsed_ms)
{
    const PracticeDefinition *practice;

    if(app == NULL || elapsed_ms <= 0 || !app->inbe.play_in_background)
        return;

    practice = practice_active(app);
    if(practice == NULL || practice->advance_elapsed == NULL)
        return;

    practice->advance_elapsed(app, elapsed_ms);
    if(practice->update != NULL && practice->is_active != NULL &&
       practice->is_active(app))
        practice->update(app);
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
