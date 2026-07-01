#include "whm_practice.h"

#include "app.h"
#include "whm_session.h"

void
whm_practice_init(InbeApp *app)
{
    if(app == NULL)
        return;
    if(app->whm.image_1.id == 0)
        app->whm.image_1 = app_load_asset_texture("practices/whm/1.jpg");
    if(app->whm.image_2.id == 0)
        app->whm.image_2 = app_load_asset_texture("practices/whm/2.jpg");
    if(app->whm.banner.id == 0)
        app->whm.banner = app_load_asset_texture("practices/whm/banner.png");
}

void
whm_practice_destroy(InbeApp *app)
{
    if(app == NULL)
        return;
    app_unload_texture(app->whm.image_1);
    app_unload_texture(app->whm.image_2);
    app_unload_texture(app->whm.banner);
}

void
whm_practice_start(InbeApp *app)
{
    if(app == NULL)
        return;
    mark_exercise_manual_seen(app, EXERCISE_WIM_HOF);
    session_start(app);
}

void
whm_practice_leave_config(InbeApp *app)
{
    (void)app;
}
