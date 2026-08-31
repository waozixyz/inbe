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
static BottomNavProps bottom_nav_last;
static BottomNavItem bottom_nav_items_last[APP_BOTTOM_NAV_CONTENT_MAX + 1];
static int save_settings_count = 0;
static int reset_settings_preview_count = 0;
static int settings_status_clear_count = 0;
static const char *generic_button_clicked_label = NULL;
static int padded_icon_click_index = -1;
static int padded_icon_draw_count = 0;
static int scroll_page_content_w_override = 0;
static int pointer_release_consumed = 0;
static Vector2 mouse_position = {0};

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
    generic_button_clicked_label = NULL;
    padded_icon_click_index = -1;
    padded_icon_draw_count = 0;
    scroll_page_content_w_override = 0;
    pointer_release_consumed = 0;
    mouse_position = (Vector2){0};
    view_width = 320;
    view_height = 560;
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
PushUIInspectSource(const char *path, int line)
{
    (void)path;
    (void)line;
}

void
PopUIInspectSource(void)
{
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

UIWidgetNode
UINodeTabBar(TabBarProps bar)
{
    UIWidgetNode node = {0};
    node.bounds.height = 44;
    (void)bar;
    return node;
}

UIWidgetNode
UINodeTitleBar(int height)
{
    UIWidgetNode node = {0};
    node.bounds.height = height > 0 ? height : 52;
    return node;
}

KeyID
Key(const char *text)
{
    (void)text;
    return 1;
}

NodeId
Screen(ColumnProps props)
{
    (void)props;
    return 1;
}

void
End(void)
{
}

int
UIGetNodeHeight(UIWidgetNode node)
{
    return (int)node.bounds.height;
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
UIText(const char *text, int x, int y, int fontSize, Color color)
{
    (void)text;
    (void)x;
    (void)y;
    (void)fontSize;
    (void)color;
}

int
MeasureUIText(const char *text, int font_size)
{
    (void)font_size;
    return text != NULL ? (int)strlen(text) * 8 : 0;
}

int
GetUITextHeight(const char *text, int font_size)
{
    (void)text;
    return font_size > 0 ? font_size : 16;
}

void
FormatLocaleText(char *out, size_t out_size, const char *key, ...)
{
    if(out == NULL || out_size == 0)
        return;
    snprintf(out, out_size, "%s", key != NULL ? key : "");
}

int
UIHref(HrefProps link)
{
    (void)link;
    return 0;
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
    return mouse_position;
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

BottomNavResult
BottomNav(BottomNavProps nav)
{
    bottom_nav_last = nav;
    if(nav.count > APP_BOTTOM_NAV_CONTENT_MAX + 1)
        nav.count = APP_BOTTOM_NAV_CONTENT_MAX + 1;
    for(int i = 0; i < nav.count; i++)
        bottom_nav_items_last[i] = nav.items[i];
    bottom_nav_last.items = bottom_nav_items_last;
    bottom_nav_draw_count++;
    return (BottomNavResult){
        .clicked_route = bottom_nav_clicked_route,
        .clicked_index = bottom_nav_clicked_route == APP_NAV_ROUTE_NONE ? -1 : 0,
        .y = 508,
        .height = 52
    };
}

UIWidgetNode
UINodeBottomNav(BottomNavProps nav)
{
    UIWidgetNode node = {0};
    node.bounds.height = 52;
    (void)nav;
    return node;
}

SidebarAccountHeaderResult
SidebarAccountHeader(SidebarAccountHeaderProps header)
{
    (void)header;
    return (SidebarAccountHeaderResult){
        .height = 138
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
StyledButton(int x, int y, int w, int h, const char *label,
             ButtonStyle style, int disabled, int *hover)
{
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)style;
    if(hover != NULL)
        *hover = 0;
    if(!disabled && generic_button_clicked_label != NULL &&
       label != NULL && strcmp(label, generic_button_clicked_label) == 0) {
        generic_button_clicked_label = NULL;
        return 1;
    }
    return 0;
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
ReorderHandle(int id, int x, int y, int w, int h, int active)
{
    (void)id;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)active;
}

void
ReorderPlaceholder(Rectangle bounds)
{
    (void)bounds;
}

UIScrollPage
BeginUIScrollPage(UIScrollPageSpec spec)
{
    return (UIScrollPage){
        .content_x = 16,
        .content_y = spec.y,
        .content_w = scroll_page_content_w_override > 0
                         ? scroll_page_content_w_override
                         : 288,
        .content_h = spec.height
    };
}

void
EndUIScrollPage(UIScrollPage page)
{
    (void)page;
}

UIScreenScaffold
BeginUIScreenScaffold(UIScreenScaffoldSpec spec)
{
    UIScreenScaffold scaffold = {0};
    int title_h = spec.title_height > 0
                      ? spec.title_height
                      : UIGetNodeHeight(UINodeTitleBar(0));
    int top_gap = spec.top_gap > 0 ? spec.top_gap : 0;

    scaffold.title_height = title_h;
    if(spec.draw_title != NULL)
        scaffold.closed = spec.draw_title(spec.title, title_h,
                                          spec.title_user_data != NULL
                                              ? spec.title_user_data
                                              : spec.user_data);
    scaffold.content_y = title_h + top_gap;
    scaffold.content_h = view_height - scaffold.content_y -
                         spec.bottom_reserved;
    if(scaffold.content_h < 0)
        scaffold.content_h = 0;
    scaffold.page = BeginUIScrollPage((UIScrollPageSpec){
        .y = scaffold.content_y,
        .height = scaffold.content_h,
        .max_content_width = spec.max_content_width,
        .min_content_width = spec.min_content_width,
        .side_padding = spec.side_padding,
        .scroll_offset = spec.scroll_offset,
        .wheel_step = spec.wheel_step,
        .scrollbar_x = spec.scrollbar_x,
        .measure_passes = spec.measure_passes,
        .content_height = spec.content_height,
        .user_data = spec.user_data
    });
    scaffold.content_x = scaffold.page.content_x;
    scaffold.content_w = scaffold.page.content_w;
    scaffold.y = scaffold.page.content_y;
    return scaffold;
}

void
EndUIScreenScaffold(UIScreenScaffold scaffold)
{
    EndUIScrollPage(scaffold.page);
}

int
UIReturnTitleBar(Texture2D return_icon, const char *title, int height)
{
    (void)return_icon;
    (void)title;
    (void)height;
    return 0;
}

int
app_draw_close_title_bar(InbeApp *app, const char *title, int height)
{
    (void)app;
    (void)title;
    (void)height;
    return 0;
}

const char *
settings_screen_tab_label(int tab)
{
    switch(tab) {
    case SETTINGS_TAB_DEVICE:
        return "Device";
    case SETTINGS_TAB_THEME:
        return "Appearance";
    case SETTINGS_TAB_ABOUT:
        return "About";
    default:
        return "Settings";
    }
}

int
GetUISmallFontSize(void)
{
    return 12;
}

int
Dropdown(int id, int x, int y, int w, int h, const char **options,
         int option_count, int *selected_index)
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
PaddedIconBtn(int id, int x, int y, int size, int padding,
              Texture2D icon, int *hover)
{
    (void)id;
    (void)x;
    (void)y;
    (void)size;
    (void)padding;
    (void)icon;
    if(hover != NULL)
        *hover = 0;
    if(padded_icon_draw_count++ == padded_icon_click_index) {
        padded_icon_click_index = -1;
        return 1;
    }
    return 0;
}

void
IconTexture(int id, int x, int y, int size, Texture2D icon, Color tint)
{
    (void)id;
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

int
UIPointerReleaseOutside(Rectangle bounds)
{
    Vector2 mouse = GetMousePosition();

    return mouse_released &&
           !pointer_release_consumed &&
           !CheckCollisionPointRec(mouse, bounds);
}

void
UIConsumeRelease(void)
{
    pointer_release_consumed = 1;
}

DismissibleOverlayResult
DismissibleOverlay(DismissibleOverlayProps overlay)
{
    DismissibleOverlayResult result = {0};
    Vector2 mouse = GetMousePosition();

    if(mouse_released && !pointer_release_consumed &&
       !overlay.dismiss_disabled &&
       !CheckCollisionPointRec(mouse, overlay.bounds)) {
        pointer_release_consumed = 1;
        result.closed = 1;
        result.outside_released = 1;
        result.release_consumed = 1;
    } else {
        result.release_consumed = pointer_release_consumed;
    }

    return result;
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
sync_account_load(KsyncAccount *account)
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
app_block_pointer_frame(InbeApp *app)
{
    if(app != NULL)
        app->input_block_frame = app->inbe.frame;
}

void
app_open_modal(InbeApp *app, UIModalType type)
{
    if(app == NULL)
        return;
    app->modal.active = 1;
    app->modal.type = type;
    app_block_pointer_frame(app);
}

void
app_close_modal(InbeApp *app)
{
    if(app == NULL)
        return;
    app->modal.active = 0;
    app->modal.type = UIModalNone;
    app_block_pointer_frame(app);
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

#include "../build/kryon/generated/src/app/app_nav.c"

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
    view_width = 720;
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
test_compact_stack_opens_sidebar_screen(void)
{
    InbeApp app = test_app();

    reset_state();
    view_width = 320;
    bottom_nav_clicked_route = APP_NAV_ROUTE_STACK;

    app_draw_bottom_nav(&app);

    expect(app.nav_sidebar_open == 1,
           "compact stack route should open sidebar");
    expect(app.inbe.screen == InbeScreenNavSidebar,
           "compact stack route should switch to sidebar screen");
}

static void
test_same_frame_modal_close_consumes_bottom_nav_click(void)
{
    InbeApp app = test_app();

    reset_state();
    app.input_block_frame = app.inbe.frame;
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
    app.input_block_frame = app.inbe.frame - 1;
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

    expect(bottom_nav_last.bottom_margin == 0,
           "bottom nav should not apply Android system nav margin inside the safe viewport");
}

static void
test_profile_hides_bottom_nav(void)
{
    InbeApp app = test_app();

    reset_state();
    app.inbe.screen = InbeScreenProfile;
    bottom_nav_clicked_route = APP_NAV_ROUTE_HABITS;

    app_draw_bottom_nav(&app);

    expect(bottom_nav_draw_count == 0,
           "profile should not draw bottom nav");
    expect(app_current_nav_route(&app) == APP_NAV_ROUTE_NONE,
           "profile should not expose a bottom nav route");
    expect(app_content_bottom_reserved(&app) == 0,
           "profile should not reserve bottom nav height");
    expect(app.inbe.screen == InbeScreenProfile,
           "profile hidden nav should not route clicks");
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

static void
test_empty_bottom_nav_draws_stack_only_bar(void)
{
    InbeApp app = test_app();

    reset_state();
    view_width = 720;
    app.main_tab = APP_MAIN_TAB_NONE;
    app.bottom_nav_route_count = 0;
    for(int i = 0; i < APP_BOTTOM_NAV_CONTENT_MAX; i++)
        app.bottom_nav_routes[i] = APP_NAV_ROUTE_NONE;
    bottom_nav_clicked_route = APP_NAV_ROUTE_STACK;

    app_draw_bottom_nav(&app);

    expect(bottom_nav_draw_count == 1,
           "empty bottom nav should draw stack-only nav bar");
    expect(bottom_nav_last.count == 1,
           "empty bottom nav should contain only stack");
    expect(bottom_nav_last.items[0].route == APP_NAV_ROUTE_STACK,
           "empty bottom nav only item should be stack");
    expect(app.nav_sidebar_open == 1,
           "empty bottom nav stack should open sidebar");
    expect(app.inbe.screen == InbeScreenStart &&
           app.main_tab == APP_MAIN_TAB_NONE,
           "empty bottom nav should stay on blank start screen");
    expect(app_content_bottom_reserved(&app) == 52,
           "empty bottom nav should reserve stack bar space");
}


static void
test_customize_nav_delete_last_does_not_add_same_frame(void)
{
    InbeApp app = test_app();

    reset_state();
    app.inbe.screen = InbeScreenCustomizeNav;
    app.main_tab = APP_MAIN_TAB_HABITS;
    app.bottom_nav_route_count = 1;
    app.bottom_nav_routes[0] = APP_NAV_ROUTE_HABITS;
    app.bottom_nav_config_route_count = 1;
    app.bottom_nav_config_routes[0] = APP_NAV_ROUTE_HABITS;
    padded_icon_click_index = 0;
    generic_button_clicked_label = "customize_nav_add";

    app_draw_customize_nav_page(&app);

    expect(app.bottom_nav_config_route_count == 0,
           "deleting last customize nav row should not also add a row");
    expect(app.bottom_nav_route_count == 0,
           "deleting last customize nav row should save an empty config");
    expect(save_settings_count == 1,
           "deleting last customize nav row should save exactly once");
    expect(generic_button_clicked_label == NULL,
           "add button click simulation should have been exercised");
}


static void
test_customize_nav_delete_icon_draws_on_narrow_rows(void)
{
    InbeApp app = test_app();

    reset_state();
    app.inbe.screen = InbeScreenCustomizeNav;
    app.bottom_nav_config_route_count = 1;
    app.bottom_nav_config_routes[0] = APP_NAV_ROUTE_HABITS;
    scroll_page_content_w_override = 180;

    app_draw_customize_nav_page(&app);

    expect(padded_icon_draw_count == 1,
           "customize nav should draw delete icon on narrow rows");
}

static void
test_bottom_nav_config_save_stays_on_customize_screen(void)
{
    InbeApp app = test_app();

    reset_state();
    app.inbe.screen = InbeScreenCustomizeNav;
    app.main_tab = APP_MAIN_TAB_NONE;
    app.bottom_nav_route_count = 0;
    for(int i = 0; i < APP_BOTTOM_NAV_CONTENT_MAX; i++)
        app.bottom_nav_routes[i] = APP_NAV_ROUTE_NONE;
    app.bottom_nav_config_route_count = 1;
    app.bottom_nav_config_routes[0] = APP_NAV_ROUTE_HABITS;

    app_save_bottom_nav_config(&app);

    expect(app.inbe.screen == InbeScreenCustomizeNav,
           "adding first nav item should stay on customize nav screen");
    expect(app.main_tab == APP_MAIN_TAB_HABITS,
           "adding first nav item should select a valid main tab");
    expect(app.bottom_nav_route_count == 1 &&
           app.bottom_nav_routes[0] == APP_NAV_ROUTE_HABITS,
           "adding first nav item should save configured route");
    expect(save_settings_count == 1,
           "adding first nav item should save settings once");
}

static void
test_open_main_tab_none_returns_blank_start(void)
{
    InbeApp app = test_app();

    reset_state();
    app.inbe.screen = InbeScreenCustomizeNav;
    app.main_tab = APP_MAIN_TAB_NONE;

    app_open_main_tab(&app, app.main_tab, 0);

    expect(app.inbe.screen == InbeScreenStart &&
           app.main_tab == APP_MAIN_TAB_NONE,
           "closing customize nav with no routes should return to blank start");
}

static void
test_compact_sidebar_close_footer_closes_to_home(void)
{
    InbeApp app = test_app();

    reset_state();
    view_width = 320;
    app.inbe.screen = InbeScreenNavSidebar;
    app.nav_sidebar_open = 1;
    app.nav_sidebar_open_frame = app.inbe.frame - 1;
    generic_button_clicked_label = "close_button";

    app_draw_bottom_nav(&app);

    expect(app.nav_sidebar_open == 0,
           "sidebar footer close should close sidebar");
    expect(app.inbe.screen == InbeScreenStart,
           "sidebar footer close should return to current home screen");
    expect(app.input_block_frame == app.inbe.frame,
           "sidebar footer close should block same-frame bottom nav clicks");
}

static void
test_overlay_sidebar_outside_release_blocks_bottom_nav(void)
{
    InbeApp app = test_app();

    reset_state();
    view_width = 720;
    app.nav_sidebar_open = 1;
    app.nav_sidebar_open_frame = app.inbe.frame - 1;
    mouse_released = 1;
    mouse_position = (Vector2){100, 20};

    app_draw_bottom_nav(&app);

    expect(app.nav_sidebar_open == 0,
           "outside release should close overlay sidebar");
    expect(pointer_release_consumed == 1,
           "outside release should be consumed");
    expect(app.input_block_frame == app.inbe.frame,
           "outside release close should block same-frame bottom nav clicks");
}

static void
test_stack_toggle_close_blocks_bottom_nav(void)
{
    InbeApp app = test_app();

    reset_state();
    view_width = 720;
    app.nav_sidebar_open = 1;

    app_apply_nav_route(&app, APP_NAV_ROUTE_STACK);

    expect(app.nav_sidebar_open == 0,
           "stack route should close an open sidebar");
    expect(app.input_block_frame == app.inbe.frame,
           "stack route close should block same-frame bottom nav clicks");
}

static void
test_consumed_pfp_release_does_not_close_sidebar(void)
{
    InbeApp app = test_app();

    reset_state();
    view_width = 720;
    app.nav_sidebar_open = 1;
    app.nav_sidebar_open_frame = app.inbe.frame - 1;
    app.modal.active = 0;
    mouse_released = 1;
    pointer_release_consumed = 1;
    mouse_position = (Vector2){100, 20};

    app_draw_bottom_nav(&app);

    expect(app.nav_sidebar_open == 1,
           "consumed pfp picker release must not close sidebar");
}

static void
test_sidebar_child_back_returns_to_compact_sidebar(void)
{
    InbeApp app = test_app();

    reset_state();
    view_width = 320;
    app.inbe.screen = InbeScreenNavSidebar;
    app.nav_sidebar_open = 1;

    app_apply_nav_route(&app, APP_NAV_ROUTE_ACCOUNT);
    expect(app.inbe.screen == InbeScreenProfile,
           "sidebar account route should open profile");
    expect(app.nav_sidebar_return_on_back == 1,
           "sidebar child route should remember compact sidebar return");

    expect(app_return_to_nav_sidebar_if_needed(&app) == 1,
           "profile back should return to compact sidebar");
    expect(app.inbe.screen == InbeScreenNavSidebar &&
           app.nav_sidebar_open == 1,
           "profile back should reopen sidebar screen");
}

static void
test_sidebar_screen_becomes_overlay_when_width_expands(void)
{
    InbeApp app = test_app();

    reset_state();
    view_width = 320;
    app.inbe.screen = InbeScreenNavSidebar;
    app.nav_sidebar_open = 1;

    view_width = 720;
    app_update_nav_sidebar_mode(&app);

    expect(app.nav_sidebar_open == 1,
           "expanded sidebar should remain open as overlay");
    expect(app.inbe.screen == InbeScreenStart,
           "expanded sidebar should return to home screen behind overlay");
}

int
main(void)
{
    test_default_bottom_nav_routes_are_habits_practice_stack();
    test_stack_opens_sidebar_without_routing();
    test_compact_stack_opens_sidebar_screen();
    test_same_frame_modal_close_consumes_bottom_nav_click();
    test_unblocked_bottom_nav_click_still_routes();
    test_edge_bottom_nav_routes_are_applied();
    test_profile_hides_bottom_nav();
    test_practice_manual_hides_bottom_nav();
    test_practice_config_hides_bottom_nav();
    test_file_dialog_hides_bottom_nav();
    test_empty_bottom_nav_draws_stack_only_bar();
    test_bottom_nav_config_save_stays_on_customize_screen();
    test_customize_nav_delete_last_does_not_add_same_frame();
    test_customize_nav_delete_icon_draws_on_narrow_rows();
    test_open_main_tab_none_returns_blank_start();
    test_compact_sidebar_close_footer_closes_to_home();
    test_overlay_sidebar_outside_release_blocks_bottom_nav();
    test_stack_toggle_close_blocks_bottom_nav();
    test_consumed_pfp_release_does_not_close_sidebar();
    test_sidebar_child_back_returns_to_compact_sidebar();
    test_sidebar_screen_becomes_overlay_when_width_expands();

    if(failures > 0) {
        fprintf(stderr, "%d app bottom nav test failure(s)\n", failures);
        return 1;
    }
    printf("app bottom nav tests passed\n");
    return 0;
}
