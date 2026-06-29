#include "app_nav.h"

#include "app.h"
#include "flint_locale.h"
#include "flint_ui.h"
#include "screens/settings/settings_screen.h"

extern int view_width;
extern int view_height;

static int android_bottom_nav_height = 0;

void
app_set_android_bottom_nav_height(int height)
{
    android_bottom_nav_height = height > 0 ? height : 0;
}

int
app_android_bottom_nav_height(void)
{
    return android_bottom_nav_height;
}

static void
app_open_main_tab(InbeApp *app, int main_tab, int persist)
{
    if(app == NULL)
        return;

    if(app->practice_tab == PRACTICE_TAB_CONFIG)
        app_leave_practice_config(app);

    app->main_tab = clampi(main_tab, APP_MAIN_TAB_HABITS, APP_MAIN_TAB_PRACTICE);
    if(app->main_tab == APP_MAIN_TAB_PRACTICE)
        app->practice_tab = PRACTICE_TAB_PLAY;
    app_switch_screen(app, app->main_tab == APP_MAIN_TAB_HABITS
                               ? InbeScreenHabits
                               : InbeScreenStart);
    if(persist)
        save_settings(app);
}

void
app_apply_nav_route(InbeApp *app, int route)
{
    if(app == NULL)
        return;

    switch(route) {
    case APP_NAV_ROUTE_PROFILE:
        if(app->practice_tab == PRACTICE_TAB_CONFIG)
            app_leave_practice_config(app);
        app->profile_view = PROFILE_VIEW_MAIN;
        app->profile_scroll = 0;
        app->sync_server_url_focused = 0;
        settings_screen_clear_status();
        app_switch_screen(app, InbeScreenProfile);
        break;
    case APP_NAV_ROUTE_PRACTICE:
        app_open_main_tab(app, APP_MAIN_TAB_PRACTICE, 1);
        break;
    case APP_NAV_ROUTE_HABITS:
        app_open_main_tab(app, APP_MAIN_TAB_HABITS, 1);
        if(app->habits.tab == HABIT_TAB_STATISTICS || app->habits.tab == HABIT_TAB_EDIT)
            app->habits.tab = app->habits.view_mode == HABIT_VIEW_WEEKLY
                                  ? HABIT_TAB_WEEKLY
                                  : HABIT_TAB_MONTHLY;
        break;
    case APP_NAV_ROUTE_SETTINGS:
        if(app->practice_tab == PRACTICE_TAB_CONFIG)
            app_leave_practice_config(app);
        reset_settings_preview(app);
        app->settings_tab = SETTINGS_TAB_SESSION;
        app->settings_scroll = 0;
        app_switch_screen(app, InbeScreenSettings);
        break;
    case APP_NAV_ROUTE_PET:
        if(app->practice_tab == PRACTICE_TAB_CONFIG)
            app_leave_practice_config(app);
        app_switch_screen(app, InbeScreenPet);
        break;
    default:
        break;
    }
}

int
app_content_bottom_reserved(const InbeApp *app)
{
    if(app == NULL)
        return 0;
    if(app_current_nav_route(app) == APP_NAV_ROUTE_NONE)
        return 0;
    return ui_bottom_nav_height() + app_android_bottom_nav_height();
}

static int
app_has_fullscreen_overlay(const InbeApp *app)
{
    if(app == NULL || !app->modal.active)
        return 0;
    return app->modal.type == UIModalPracticeManual ||
           app->modal.type == UIModalPracticeConfig ||
           app->modal.type == UIModalEditProgressiveStartSpeed;
}

static const char *
app_nav_route_label(int route)
{
    switch(route) {
    case APP_NAV_ROUTE_PROFILE: return locale_get("tab_profile");
    case APP_NAV_ROUTE_HABITS: return locale_get("tab_habits");
    case APP_NAV_ROUTE_PRACTICE: return locale_get("tab_practice");
    case APP_NAV_ROUTE_PET: return locale_get("tab_pet");
    case APP_NAV_ROUTE_SETTINGS: return locale_get("tab_settings");
    default: break;
    }
    return "";
}

static Texture2D
app_nav_route_icon(InbeApp *app, int route)
{
    if(app == NULL)
        return (Texture2D){0};
    switch(route) {
    case APP_NAV_ROUTE_PROFILE: return app->icons[UI_ICON_TYPE_PROFILE];
    case APP_NAV_ROUTE_HABITS: return app->icons[UI_ICON_TYPE_HABIT];
    case APP_NAV_ROUTE_PRACTICE: return app->icons[UI_ICON_TYPE_AMEN];
    case APP_NAV_ROUTE_PET: return app->icons[UI_ICON_TYPE_PET];
    case APP_NAV_ROUTE_SETTINGS: return app->icons[UI_ICON_TYPE_GEAR];
    default: break;
    }
    return (Texture2D){0};
}

int
app_current_nav_route(const InbeApp *app)
{
    if(app == NULL)
        return APP_NAV_ROUTE_NONE;
    switch(app->inbe.screen) {
    case InbeScreenProfile:
        return APP_NAV_ROUTE_PROFILE;
    case InbeScreenStart:
        return APP_NAV_ROUTE_PRACTICE;
    case InbeScreenHabits:
        return APP_NAV_ROUTE_HABITS;
    case InbeScreenPet:
        return APP_NAV_ROUTE_PET;
    case InbeScreenSettings:
        return APP_NAV_ROUTE_SETTINGS;
    default:
        break;
    }
    return APP_NAV_ROUTE_NONE;
}

static int
app_nav_route_active(const InbeApp *app, int route)
{
    if(app == NULL)
        return 0;
    if(route == APP_NAV_ROUTE_PROFILE)
        return app->inbe.screen == InbeScreenProfile;
    if(route == APP_NAV_ROUTE_PRACTICE)
        return app->inbe.screen == InbeScreenStart;
    if(route == APP_NAV_ROUTE_HABITS)
        return app->inbe.screen == InbeScreenHabits ||
               app->inbe.screen == InbeScreenHabitEdit ||
               app->inbe.screen == InbeScreenHabitSessionEdit;
    if(route == APP_NAV_ROUTE_PET)
        return app->inbe.screen == InbeScreenPet;
    if(route == APP_NAV_ROUTE_SETTINGS)
        return app->inbe.screen == InbeScreenSettings;
    return 0;
}

static int
app_should_draw_bottom_nav(const InbeApp *app)
{
    if(app == NULL)
        return 0;
    if(app_has_fullscreen_overlay(app))
        return 0;
    return app_current_nav_route(app) != APP_NAV_ROUTE_NONE;
}

int
app_fullscreen_bottom_reserved(const InbeApp *app)
{
    if(app == NULL || app_should_draw_bottom_nav(app))
        return 0;
    return app_android_bottom_nav_height();
}

void
app_draw_bottom_nav(InbeApp *app)
{
    enum { BOTTOM_NAV_ROUTE_COUNT = 5 };
    const int routes[BOTTOM_NAV_ROUTE_COUNT] = {
        APP_NAV_ROUTE_PROFILE,
        APP_NAV_ROUTE_HABITS,
        APP_NAV_ROUTE_PRACTICE,
        APP_NAV_ROUTE_PET,
        APP_NAV_ROUTE_SETTINGS
    };
    FlintUIBottomNavItem items[BOTTOM_NAV_ROUTE_COUNT];
    FlintUIBottomNavResult result;

    if(app_android_bottom_nav_height() > 0)
        DrawRectangle(0, view_height - app_android_bottom_nav_height(),
                      view_width, app_android_bottom_nav_height(), BLACK);
    if(!app_should_draw_bottom_nav(app))
        return;
    for(int i = 0; i < BOTTOM_NAV_ROUTE_COUNT; i++) {
        int route = routes[i];
        items[i] = (FlintUIBottomNavItem){
            route,
            app_nav_route_label(route),
            app_nav_route_icon(app, route),
            app_nav_route_active(app, route),
            app->modal.active
        };
    }
    result = ui_draw_bottom_nav((FlintUIBottomNav){
        .view_width = view_width,
        .view_height = view_height,
        .bottom_margin = app_android_bottom_nav_height(),
        .count = BOTTOM_NAV_ROUTE_COUNT,
        .items = items
    });
    if(result.clicked_route != APP_NAV_ROUTE_NONE)
        app_apply_nav_route(app, result.clicked_route);
}
