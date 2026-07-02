#include "app/app.h"
#include "app/app_nav.h"
#include "flint_locale.h"
#include "flint_ui.h"
#include "screens/settings/settings_screen.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

int view_width = 320;
int view_height = 560;

static int failures = 0;
static int mouse_released = 0;
static int bottom_nav_draw_count = 0;
static int bottom_nav_clicked_route = APP_NAV_ROUTE_NONE;
static int save_settings_count = 0;
static int reset_settings_preview_count = 0;
static int settings_status_clear_count = 0;

static void
expect(int condition, const char *message)
{
    if(!condition) {
        fprintf(stderr, "FAIL %s\n", message);
        failures++;
    }
}

static void
reset_state(void)
{
    mouse_released = 0;
    bottom_nav_draw_count = 0;
    bottom_nav_clicked_route = APP_NAV_ROUTE_NONE;
    save_settings_count = 0;
    reset_settings_preview_count = 0;
    settings_status_clear_count = 0;
    view_width = 320;
    view_height = 560;
    app_set_android_bottom_nav_height(0);
}

bool
IsMouseButtonReleased(int button)
{
    return button == MOUSE_BUTTON_LEFT && mouse_released;
}

void
DrawRectangle(int posX, int posY, int width, int height, Color color)
{
    (void)posX;
    (void)posY;
    (void)width;
    (void)height;
    (void)color;
}

int
ui_bottom_nav_height(void)
{
    return 40;
}

FlintUIBottomNavResult
ui_draw_bottom_nav(FlintUIBottomNav nav)
{
    (void)nav;
    bottom_nav_draw_count++;
    return (FlintUIBottomNavResult){
        .clicked_route = bottom_nav_clicked_route,
        .clicked_index = bottom_nav_clicked_route == APP_NAV_ROUTE_NONE ? -1 : 0,
        .y = 520,
        .height = 40
    };
}

const char *
locale_get(const char *key)
{
    return key != NULL ? key : "";
}

int
clampi(int value, int min, int max)
{
    if(value < min)
        return min;
    if(value > max)
        return max;
    return value;
}

void
app_switch_screen(InbeApp *app, int screen)
{
    if(app != NULL)
        app->inbe.screen = screen;
}

void
app_leave_practice_config(InbeApp *app)
{
    if(app != NULL)
        app->practice_tab = PRACTICE_TAB_PLAY;
}

void
save_settings(InbeApp *app)
{
    (void)app;
    save_settings_count++;
}

void
reset_settings_preview(InbeApp *app)
{
    (void)app;
    reset_settings_preview_count++;
}

void
settings_screen_clear_status(void)
{
    settings_status_clear_count++;
}

#include "../src/app/app_nav.c"

static InbeApp
test_app(void)
{
    InbeApp app;

    memset(&app, 0, sizeof(app));
    app.inbe.screen = InbeScreenStart;
    app.inbe.frame = 42;
    app.practice_tab = PRACTICE_TAB_PLAY;
    app.main_tab = APP_MAIN_TAB_PRACTICE;
    return app;
}

static void
test_same_frame_modal_close_consumes_bottom_nav_click(void)
{
    InbeApp app = test_app();

    reset_state();
    app.modal_input_block_frame = app.inbe.frame;
    mouse_released = 1;
    bottom_nav_clicked_route = APP_NAV_ROUTE_SETTINGS;

    app_draw_bottom_nav(&app);

    expect(bottom_nav_draw_count == 0,
           "same-frame modal close must skip bottom nav draw");
    expect(app.inbe.screen == InbeScreenStart,
           "same-frame modal close must not route to settings");
    expect(reset_settings_preview_count == 0,
           "same-frame modal close must not run settings route side effects");
}

static void
test_unblocked_bottom_nav_click_still_routes(void)
{
    InbeApp app = test_app();

    reset_state();
    app.modal_input_block_frame = app.inbe.frame - 1;
    mouse_released = 1;
    bottom_nav_clicked_route = APP_NAV_ROUTE_SETTINGS;

    app_draw_bottom_nav(&app);

    expect(bottom_nav_draw_count == 1,
           "unblocked frame should draw bottom nav");
    expect(app.inbe.screen == InbeScreenSettings,
           "unblocked bottom nav click should route to settings");
    expect(reset_settings_preview_count == 1,
           "settings route should reset preview");
}

static void
test_practice_manual_hides_bottom_nav(void)
{
    InbeApp app = test_app();

    reset_state();
    app.practice_tab = PRACTICE_TAB_MANUAL;
    bottom_nav_clicked_route = APP_NAV_ROUTE_SETTINGS;

    app_draw_bottom_nav(&app);

    expect(bottom_nav_draw_count == 0,
           "practice manual should not draw bottom nav");
    expect(app_current_nav_route(&app) == APP_NAV_ROUTE_NONE,
           "practice manual should not expose a bottom nav route");
    expect(app_content_bottom_reserved(&app) == 0,
           "practice manual should not reserve bottom nav height");
    expect(app.inbe.screen == InbeScreenStart,
           "practice manual hidden nav should not route clicks");
}

static void
test_practice_config_hides_bottom_nav(void)
{
    InbeApp app = test_app();

    reset_state();
    app.practice_tab = PRACTICE_TAB_CONFIG;
    bottom_nav_clicked_route = APP_NAV_ROUTE_SETTINGS;

    app_draw_bottom_nav(&app);

    expect(bottom_nav_draw_count == 0,
           "practice config should not draw bottom nav");
    expect(app_current_nav_route(&app) == APP_NAV_ROUTE_NONE,
           "practice config should not expose a bottom nav route");
    expect(app_content_bottom_reserved(&app) == 0,
           "practice config should not reserve bottom nav height");
    expect(app.inbe.screen == InbeScreenStart,
           "practice config hidden nav should not route clicks");
}

int
main(void)
{
    test_same_frame_modal_close_consumes_bottom_nav_click();
    test_unblocked_bottom_nav_click_still_routes();
    test_practice_manual_hides_bottom_nav();
    test_practice_config_hides_bottom_nav();

    if(failures > 0) {
        fprintf(stderr, "%d app bottom nav test failure(s)\n", failures);
        return 1;
    }
    printf("app bottom nav tests passed\n");
    return 0;
}
