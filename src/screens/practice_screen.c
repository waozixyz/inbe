#include "practice_screen.h"
#include "app.h"
#include "locale.h"

void
on_practice_tab_click(void *user_data)
{
    InbeApp *app = user_data;
    extern void app_request_bottom_tab(InbeApp *app, int bottom_tab);
    enum { APP_BOTTOM_TAB_PRACTICE = 1 };
    app_request_bottom_tab(app, APP_BOTTOM_TAB_PRACTICE);
}

int
practice_activity_count_for_tab(int tab)
{
    (void)tab;
    return EXERCISE_COUNT;
}

int
practice_activity_for_tab(int tab, int index)
{
    (void)tab;
    if(index < 0)
        return EXERCISE_WIM_HOF;
    if(index >= EXERCISE_COUNT)
        return EXERCISE_COUNT - 1;
    return index;
}

const char *
practice_activity_label(int exercise)
{
    switch(exercise) {
    case EXERCISE_MEDITATION:
        return locale_get("exercise_meditation");
    case EXERCISE_SUN_SALUTATION:
        return "Sun Salutation";
    case EXERCISE_7_MINUTE_WORKOUT:
        return "7-Minute Workout";
    case EXERCISE_WIM_HOF:
    default:
        return locale_get("exercise_wim_hof");
    }
}

int
practice_activity_index_for_tab(int tab, int exercise)
{
    (void)tab;
    if(exercise < 0 || exercise >= EXERCISE_COUNT)
        return EXERCISE_WIM_HOF;
    return exercise;
}

void
practice_clamp_activity_to_tab(InbeApp *app)
{
    if(app == NULL)
        return;
    if(app->exercise_type < 0 || app->exercise_type >= EXERCISE_COUNT)
        app->exercise_type = EXERCISE_WIM_HOF;
}
