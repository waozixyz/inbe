#include "manual_screen.h"

#include "app.h"
#include "practices/practice_registry.h"

void
manual_screen_reset_layouts(InbeApp *app)
{
    if(app == NULL)
        return;
    app->manual_scroll = 0;
}

void
manual_screen_close_tutorial(InbeApp *app, int mark_seen)
{
    const PracticeDefinition *practice;

    if(app == NULL)
        return;
    practice = practice_get(app->exercise_type);
    if(practice->close_manual != NULL)
        practice->close_manual(app, mark_seen);
}

void
manual_screen_draw(InbeApp *app)
{
    const PracticeDefinition *practice;

    if(app == NULL)
        return;
    practice = practice_get(app->exercise_type);
    if(practice->draw_manual != NULL)
        practice->draw_manual(app);
}
