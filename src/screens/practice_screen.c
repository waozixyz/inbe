#include "practice_screen.h"
#include "app.h"
#include "practices/practice_registry.h"

int
practice_activity_count_for_tab(int tab)
{
    (void)tab;
    return practice_count();
}

int
practice_activity_for_tab(int tab, int index)
{
    (void)tab;
    if(index < 0)
        return EXERCISE_WIM_HOF;
    if(index >= practice_count())
        return practice_count() - 1;
    return index;
}

const char *
practice_activity_label(int exercise)
{
    return practice_label(exercise);
}

int
practice_activity_index_for_tab(int tab, int exercise)
{
    (void)tab;
    return practice_clamp_id(exercise);
}

void
practice_clamp_activity_to_tab(InbeApp *app)
{
    if(app == NULL)
        return;
    app->exercise_type = practice_clamp_id(app->exercise_type);
}
