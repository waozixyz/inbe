#include "meditation_practice.h"

#include "app.h"
#include "meditation_music.h"

void
meditation_practice_init(InbeApp *app)
{
    if(app != NULL && app->meditation.image_1.id == 0)
        app->meditation.image_1 = app_load_asset_texture("practices/meditation/1.jpg");
    meditation_music_init(app);
}

void
meditation_practice_destroy(InbeApp *app)
{
    if(app != NULL)
        app_unload_texture(app->meditation.image_1);
    meditation_music_unload(app);
}

void
meditation_practice_start(InbeApp *app)
{
    if(app == NULL)
        return;
    mark_exercise_manual_seen(app, EXERCISE_MEDITATION);
    meditation_start_configured(app);
}

void
meditation_practice_update(InbeApp *app)
{
    meditation_music_update(app);
}

void
meditation_practice_leave_config(InbeApp *app)
{
    meditation_music_unload(app);
}
