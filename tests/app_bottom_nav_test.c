#include "app/app.h"
#include "app/app_nav.h"
#include "locale.h"
#include "ui.h"
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
static UIBottomNav bottom_nav_last;
static UIBottomNavItem bottom_nav_items_last[APP_BOTTOM_NAV_CONTENT_MAX + 1];
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
    memset(&bottom_nav_last, 0, sizeof(bottom_nav_last));
    memset(bottom_nav_items_last, 0, sizeof(bottom_nav_items_last));
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

void
DrawLine(int startPosX, int startPosY, int endPosX, int endPosY, Color color)
{
    (void)startPosX;
    (void)startPosY;
    (void)endPosX;
    (void)endPosY;
    (void)color;
}

void
DrawRectangleRounded(Rectangle rec, float roundness, int segments, Color color)
{
    (void)rec;
    (void)roundness;
    (void)segments;
    (void)color;
}

void
DrawCircle(int centerX, int centerY, float radius, Color color)
{
    (void)centerX;
    (void)centerY;
    (void)radius;
    (void)color;
}

void
DrawCircleLines(int centerX, int centerY, float radius, Color color)
{
    (void)centerX;
    (void)centerY;
    (void)radius;
    (void)color;
}

Color
GetThemeSurface(void)
{
    return (Color){0};
}

Color
GetThemeBackground(void)
{
    return (Color){0};
}

Color
GetThemeButton(void)
{
    return (Color){0};
}

Color
GetThemeIcon(void)
{
    return (Color){0};
}

Color
GetThemeText(void)
{
    return (Color){0};
}

Color
DarkenUIColor(Color color, int amount)
{
    (void)amount;
    return color;
}

Color
LightenUIColor(Color color, int amount)
{
    (void)amount;
    return color;
}

int
ScaleUIPx(int px)
{
    return px;
}

int
GetUITabBarHeight(void)
{
    return 44;
}

int
GetUIFontSize(void)
{
    return 16;
}

int
GetUIControlTextY(const char *text, int y, int h, int font)
{
    (void)text;
    (void)font;
    return y + h / 2;
}

void
DrawUIText(const char *text, int x, int y, int fontSize, Color color)
{
    (void)text;
    (void)x;
    (void)y;
    (void)fontSize;
    (void)color;
}

bool
CheckCollisionPointRec(Vector2 point, Rectangle rec)
{
    return point.x >= rec.x && point.x <= rec.x + rec.width &&
           point.y >= rec.y && point.y <= rec.y + rec.height;
}

Vector2
GetMousePosition(void)
{
    return (Vector2){0};
}

Vector2
GetScreenToWorld2D(Vector2 position, Camera2D camera)
{
    (void)camera;
    return position;
}

bool
IsMouseButtonPressed(int button)
{
    (void)button;
    return false;
}

int
GetUIBottomNavHeight(void)
{
    return 52;
}

UIBottomNavResult
DrawUIBottomNav(UIBottomNav nav)
{
    bottom_nav_last = nav;
    if(nav.count > APP_BOTTOM_NAV_CONTENT_MAX + 1)
        nav.count = APP_BOTTOM_NAV_CONTENT_MAX + 1;
    for(int i = 0; i < nav.count; i++)
        bottom_nav_items_last[i] = nav.items[i];
    bottom_nav_last.items = bottom_nav_items_last;
    bottom_nav_draw_count++;
    return (UIBottomNavResult){
        .clicked_route = bottom_nav_clicked_route,
        .clicked_index = bottom_nav_clicked_route == APP_NAV_ROUTE_NONE ? -1 : 0,
        .y = 508,
        .height = 52
    };
}

UIScrollView
BeginUIScrollContainer(UIScrollArea area)
{
    return (UIScrollView){
        .content_x = area.content_x,
        .content_y = (int)area.bounds.y,
        .content_w = area.content_width,
        .viewport_h = (int)area.bounds.height,
        .content_h = area.content_height
    };
}

void
EndUIScrollContainer(UIScrollArea area, UIScrollView view)
{
    (void)area;
    (void)view;
}

int
DrawUISectionLabel(UISectionLabel label, int x, int y)
{
    (void)label;
    (void)x;
    (void)y;
    return 0;
}

int
DrawUIGenericButton(int x, int y, int w, int h, const char *label,
                    UIButtonStyle style, int disabled, int *hover)
{
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)label;
    (void)style;
    (void)disabled;
    if(hover != NULL)
        *hover = 0;
    return 0;
}

UIBottomNavConfigResult
DrawUIBottomNavConfigModal(UIBottomNavConfigModal modal)
{
    (void)modal;
    return (UIBottomNavConfigResult){0};
}

UIReorderListResult
UpdateUIReorderList(UIReorderList list)
{
    (void)list;
    return (UIReorderListResult){
        .from_index = -1,
        .to_index = -1,
        .active_index = -1,
        .target_index = -1
    };
}

void
DrawUIReorderHandle(int x, int y, int w, int h, int active)
{
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)active;
}

void
DrawUIReorderPlaceholder(Rectangle bounds)
{
    (void)bounds;
}

UIScrollPage
BeginUIScrollPage(UIScrollPageSpec spec)
{
    return (UIScrollPage){
        .content_x = 16,
        .content_y = spec.y,
        .content_w = 288,
        .content_h = spec.height
    };
}

void
EndUIScrollPage(UIScrollPage page)
{
    (void)page;
}

int
DrawUIReturnTitleBar(Texture2D return_icon, const char *title, int height)
{
    (void)return_icon;
    (void)title;
    (void)height;
    return 0;
}

int
GetUISmallFontSize(void)
{
    return 12;
}

int
DrawUIDropdownButton(int id, int x, int y, int w, int h,
                     const char **options, int option_count,
                     int *selected_index)
{
    (void)id;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)options;
    (void)option_count;
    (void)selected_index;
    return 0;
}

int
DrawUIDropdownMenu(int id)
{
    (void)id;
    return 0;
}

void
SetUIDropdownClipTop(int y)
{
    (void)y;
}

void
SetUIDropdownClipBottom(int y)
{
    (void)y;
}

int
DrawUIPaddedIconBtn(int x, int y, int size, int padding,
                    Texture2D icon, int *hover)
{
    (void)x;
    (void)y;
    (void)size;
    (void)padding;
    (void)icon;
    if(hover != NULL)
        *hover = 0;
    return 0;
}

void
DrawUIIconTexture(int x, int y, int size, Texture2D icon, Color tint)
{
    (void)x;
    (void)y;
    (void)size;
    (void)icon;
    (void)tint;
}

void
ClearUIInputCaptures(void)
{
}

const char *
GetLocaleText(const char *key)
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

AppRoute
app_current_route(const InbeApp *app)
{
    AppRoute route;

    memset(&route, 0, sizeof(route));
    if(app == NULL)
        return route;
    route.screen = app->inbe.screen;
    route.exercise_type = app->exercise_type;
    route.practice_tab = app->practice_tab;
    route.practice_config_tab = app->practice_config_tab;
    route.settings_tab = app->settings_tab;
    route.profile_view = app->profile_view;
    route.profile_tab = app->profile_tab;
    route.habits_screen_mode = app->habits.screen_mode;
    route.habits_tab = app->habits.tab;
    return route;
}

void
app_switch_route(InbeApp *app, AppRoute route)
{
    if(app == NULL)
        return;
    app->inbe.screen = route.screen;
    app->exercise_type = route.exercise_type;
    app->practice_tab = route.practice_tab;
    app->practice_config_tab = route.practice_config_tab;
    app->settings_tab = route.settings_tab;
    app->profile_view = route.profile_view;
    app->profile_tab = route.profile_tab;
    app->habits.screen_mode = route.habits_screen_mode;
    app->habits.tab = route.habits_tab;
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

int
sync_account_load(InbeSyncAccount *account)
{
    (void)account;
    return 0;
}

const char *
storage_get_setting_text(const char *key)
{
    (void)key;
    return "";
}

void
app_open_modal(InbeApp *app, UIModalType type)
{
    if(app == NULL)
        return;
    app->modal.active = 1;
    app->modal.type = type;
    app->modal_input_block_frame = app->inbe.frame;
}

void
app_close_modal(InbeApp *app)
{
    if(app == NULL)
        return;
    app->modal.active = 0;
    app->modal.type = UIModalNone;
    app->modal_input_block_frame = app->inbe.frame;
}

int
profile_social_friends_count(InbeApp *app)
{
    (void)app;
    return 0;
}

int
profile_social_pending_count(InbeApp *app)
{
    (void)app;
    return 0;
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
    app_reset_bottom_nav_routes(&app);
    return app;
}

static void
test_default_bottom_nav_routes_are_habits_practice_stack(void)
{
    InbeApp app = test_app();

    reset_state();
    app_draw_bottom_nav(&app);

    expect(bottom_nav_draw_count == 1,
           "default bottom nav should draw");
    expect(bottom_nav_last.count == 3,
           "default bottom nav should have three items");
    expect(bottom_nav_last.items[0].route == APP_NAV_ROUTE_HABITS,
           "default first bottom nav item should be habits");
    expect(bottom_nav_last.items[1].route == APP_NAV_ROUTE_PRACTICE,
           "default second bottom nav item should be practice");
    expect(bottom_nav_last.items[2].route == APP_NAV_ROUTE_STACK,
           "default third bottom nav item should be stack");
}

static void
test_stack_opens_sidebar_without_routing(void)
{
    InbeApp app = test_app();

    reset_state();
    bottom_nav_clicked_route = APP_NAV_ROUTE_STACK;

    app_draw_bottom_nav(&app);

    expect(bottom_nav_draw_count == 1,
           "stack route should draw bottom nav");
    expect(app.nav_sidebar_open == 1,
           "stack route should open sidebar");
    expect(app.inbe.screen == InbeScreenStart,
           "stack route should not change screens");
    expect(reset_settings_preview_count == 0,
           "stack route should not run settings route side effects");
}

static void
test_same_frame_modal_close_consumes_bottom_nav_click(void)
{
    InbeApp app = test_app();

    reset_state();
    app.modal_input_block_frame = app.inbe.frame;
    mouse_released = 1;
    bottom_nav_clicked_route = APP_NAV_ROUTE_HABITS;

    app_draw_bottom_nav(&app);

    expect(bottom_nav_draw_count == 0,
           "same-frame modal close must skip bottom nav draw");
    expect(app.inbe.screen == InbeScreenStart,
           "same-frame modal close must not route");
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
    bottom_nav_clicked_route = APP_NAV_ROUTE_HABITS;

    app_draw_bottom_nav(&app);

    expect(bottom_nav_draw_count == 1,
           "unblocked frame should draw bottom nav");
    expect(app.inbe.screen == InbeScreenHabits,
           "unblocked bottom nav click should route to habits");
    expect(reset_settings_preview_count == 0,
           "habits route should not reset settings preview");
    expect(app_content_bottom_reserved(&app) == 52,
           "bottom nav should reserve larger touch height");
    expect(bottom_nav_last.bottom_margin == 0,
           "bottom nav should preserve zero Android margin");
}

static void
test_edge_bottom_nav_routes_are_applied(void)
{
    InbeApp app = test_app();

    reset_state();
    bottom_nav_clicked_route = APP_NAV_ROUTE_HABITS;
    app_draw_bottom_nav(&app);

    expect(bottom_nav_draw_count == 1,
           "habits edge route should draw bottom nav");
    expect(app.inbe.screen == InbeScreenHabits,
           "habits edge route should switch to habits");

    app = test_app();
    reset_state();
    bottom_nav_clicked_route = APP_NAV_ROUTE_HABITS;
    app_set_android_bottom_nav_height(24);
    app_draw_bottom_nav(&app);

    expect(bottom_nav_draw_count == 1,
           "habits edge route should draw bottom nav with system margin");
    expect(app.inbe.screen == InbeScreenHabits,
           "habits edge route should switch to habits with system margin");
    expect(bottom_nav_last.bottom_margin == 24,
           "bottom nav should preserve Android system nav margin");
}

static void
test_practice_manual_hides_bottom_nav(void)
{
    InbeApp app = test_app();

    reset_state();
    app.practice_tab = PRACTICE_TAB_MANUAL;
    bottom_nav_clicked_route = APP_NAV_ROUTE_HABITS;

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
    bottom_nav_clicked_route = APP_NAV_ROUTE_HABITS;

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

static void
test_file_dialog_hides_bottom_nav(void)
{
    InbeApp app = test_app();

    reset_state();
    app.file_dialog_active = 1;
    bottom_nav_clicked_route = APP_NAV_ROUTE_HABITS;

    app_draw_bottom_nav(&app);

    expect(bottom_nav_draw_count == 0,
           "active file dialog should not draw bottom nav");
    expect(app.inbe.screen == InbeScreenStart,
           "active file dialog should not route bottom nav clicks");
}

int
main(void)
{
    test_default_bottom_nav_routes_are_habits_practice_stack();
    test_stack_opens_sidebar_without_routing();
    test_same_frame_modal_close_consumes_bottom_nav_click();
    test_unblocked_bottom_nav_click_still_routes();
    test_edge_bottom_nav_routes_are_applied();
    test_practice_manual_hides_bottom_nav();
    test_practice_config_hides_bottom_nav();
    test_file_dialog_hides_bottom_nav();

    if(failures > 0) {
        fprintf(stderr, "%d app bottom nav test failure(s)\n", failures);
        return 1;
    }
    printf("app bottom nav tests passed\n");
    return 0;
}
