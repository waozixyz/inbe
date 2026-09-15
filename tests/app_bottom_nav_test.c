#include "app/app.h"
#include "app/app_nav.h"
#include "locale.h"
#include "ui_tree.h"
#include "screens/settings/settings_screen.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

int view_width = 320;
int view_height = 560;

static int failures = 0;
static int desktop_mode = 0;
static int elist_label_count = 0;
static int mouse_released = 0;
static int bottom_nav_draw_count = 0;
static int bottom_nav_clicked_route = APP_NAV_ROUTE_NONE;
static NavigationBarProps bottom_nav_last;
static NavigationBarItem bottom_nav_items_last[APP_BOTTOM_NAV_CONTENT_MAX + 1];
static int save_settings_count = 0;
static int reset_settings_preview_count = 0;
static int settings_status_clear_count = 0;
static const char *generic_button_clicked_label = NULL;
static int invisible_button_clicked_id = -1;
static int icon_button_click_index = -1;
static int icon_button_draw_count = 0;
static Rectangle rail_bounds[5];
static int rail_bounds_count = 0;
static int route_request_count = 0;
static int scroll_page_content_w_override = 0;
static int pointer_release_consumed = 0;
static Vector2 mouse_position = {0};

#define Text TestText

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
    desktop_mode = 0;
    elist_label_count = 0;
    mouse_released = 0;
    bottom_nav_draw_count = 0;
    bottom_nav_clicked_route = APP_NAV_ROUTE_NONE;
    memset(&bottom_nav_last, 0, sizeof(bottom_nav_last));
    memset(bottom_nav_items_last, 0, sizeof(bottom_nav_items_last));
    save_settings_count = 0;
    reset_settings_preview_count = 0;
    settings_status_clear_count = 0;
    generic_button_clicked_label = NULL;
    invisible_button_clicked_id = -1;
    icon_button_click_index = -1;
    icon_button_draw_count = 0;
    rail_bounds_count = 0;
    route_request_count = 0;
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
DrawRectangleRoundedLinesEx(Rectangle rec, float roundness, int segments,
                            float lineThick, Color color)
{
    (void)rec;
    (void)roundness;
    (void)segments;
    (void)lineThick;
    (void)color;
}

void
DrawRectangleGradientH(int posX, int posY, int width, int height,
                       Color left, Color right)
{
    (void)posX;
    (void)posY;
    (void)width;
    (void)height;
    (void)left;
    (void)right;
}

void
DrawTexturePro(Texture2D texture, Rectangle source, Rectangle dest,
               Vector2 origin, float rotation, Color tint)
{
    (void)texture;
    (void)source;
    (void)dest;
    (void)origin;
    (void)rotation;
    (void)tint;
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
GetThemeButtonHover(void)
{
    return (Color){0};
}

Color
GetThemeButtonText(void)
{
    return (Color){255, 255, 255, 255};
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

Color
Fade(Color color, float alpha)
{
    color.a = (unsigned char)(alpha <= 0.0f ? 0.0f :
                              alpha >= 1.0f ? 255.0f : alpha * 255.0f);
    return color;
}

int
Scale(int px)
{
    return px;
}

int
TabBar(TabBarProps bar)
{
    (void)bar;
    return 0;
}

int
TitleBar(TitleBarProps title_bar)
{
    (void)title_bar;
    return 0;
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

void TestText(TextProps props) __asm__("Text");

void
TestText(TextProps props)
{
    if(props.text != NULL && strcmp(props.text, "tab_elist") == 0) {
        elist_label_count++;
    }
}

int
MeasureUIText(const char *text, int font_size)
{
    (void)font_size;
    return text != NULL ? (int)strlen(text) * 8 : 0;
}

int
TextWidth(const char *text, int font)
{
    return MeasureUIText(text, font);
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

int
IsUIDesktopMode(void)
{
    return desktop_mode;
}

int
UIInputCapturesClick(Vector2 point)
{
    (void)point;
    return 0;
}


void
MarkUIClickable(void)
{
}

NavigationBarResult
NavigationBar(NavigationBarProps nav)
{
    bottom_nav_last = nav;
    if(nav.count > APP_BOTTOM_NAV_CONTENT_MAX + 1)
        nav.count = APP_BOTTOM_NAV_CONTENT_MAX + 1;
    for(int i = 0; i < nav.count; i++)
        bottom_nav_items_last[i] = nav.items[i];
    bottom_nav_last.items = bottom_nav_items_last;
    bottom_nav_draw_count++;
    return (NavigationBarResult){
        .clicked_route = bottom_nav_clicked_route,
        .clicked_index = bottom_nav_clicked_route == APP_NAV_ROUTE_NONE ? -1 : 0,
        .y = 508,
        .height = 80
    };
}

int
GetNodeHeightById(int id)
{
    (void)id;
    return 0;
}




int
Button(ButtonProps props)
{
    if(props.label != NULL && strcmp(props.label, "tab_elist") == 0)
        elist_label_count++;

    if((props.id == 6690 || (props.id >= 6600 && props.id < 6690)) &&
       rail_bounds_count < 5) {
        rail_bounds[rail_bounds_count++] = props.bounds;
    }
    if(props.disabled)
        return 0;
    if(props.icon_only && icon_button_draw_count++ == icon_button_click_index) {
        icon_button_click_index = -1;
        return 1;
    }
    if((int)props.id == invisible_button_clicked_id) {
        invisible_button_clicked_id = -1;
        return 1;
    }
    if(generic_button_clicked_label != NULL && props.label != NULL &&
       strcmp(props.label, generic_button_clicked_label) == 0) {
        generic_button_clicked_label = NULL;
        return 1;
    }
    if(mouse_released && !pointer_release_consumed &&
       CheckCollisionPointRec(mouse_position, props.bounds)) {
        ConsumeRelease();
        return 1;
    }
    return 0;
}

ReorderListResult
UpdateReorderList(ReorderList list)
{
    (void)list;
    return (ReorderListResult){
        .from_index = -1,
        .to_index = -1,
        .active_index = -1,
        .target_index = -1
    };
}

void
AppReorderHandle(int x, int y, int w, int h, int active)
{
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)active;
}

void
AppReorderPlaceholder(Rectangle bounds)
{
    (void)bounds;
}

int
MeasureTextWidth(const char *text, int font_size, const char *typeface)
{
    (void)typeface;
    if(text == NULL)
        return 0;
    return (int)strlen(text) * (font_size > 0 ? font_size / 2 : 8);
}

Color
DarkenColor(Color color, int amount)
{
    (void)amount;
    return color;
}

void
PushInspectSource(const char *path, int line)
{
    (void)path;
    (void)line;
}

void
PopInspectSource(void)
{
}

Color
LightenColor(Color color, int amount)
{
    (void)amount;
    return color;
}

int
IsDesktopMode(void)
{
    return desktop_mode;
}

int
ReleaseConsumed(void)
{
    return pointer_release_consumed;
}

int
GetPageSidePadding(void)
{
    return 0;
}

void
GetCenteredColumn(int max_w, int side_pad, int *x, int *w)
{
    if(x == NULL || w == NULL)
        return;
    *w = view_width - side_pad * 2;
    if(max_w > 0 && *w > max_w)
        *w = max_w;
    *x = (view_width - *w) / 2;
}

Rectangle
ScrollScope(Rectangle bounds, int content_height, int *scroll_offset)
{
    int content_w = scroll_page_content_w_override > 0
                        ? scroll_page_content_w_override
                        : 288;

    (void)content_height;
    (void)scroll_offset;
    bounds.x = 16;
    bounds.width = (float)content_w;
    return bounds;
}

void
ScrollEndScope(void)
{
}

int
ReturnTitleBar(Texture2D return_icon, const char *title, int height)
{
    (void)return_icon;
    (void)title;
    (void)height;
    return 0;
}

int
app_draw_close_title_bar(InnerBreeze*app, const char *title, int height)
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
GetSmallFontSize(void)
{
    return 12;
}

int
Dropdown(DropdownProps props)
{
    (void)props;
    return 0;
}


void
Icon(int id, int x, int y, int size, IconType icon, Color tint)
{
    (void)id;
    (void)x;
    (void)y;
    (void)size;
    (void)icon;
    (void)tint;
}

void
PushInputCapture(Rectangle bounds, int allow_inside)
{
    (void)bounds;
    (void)allow_inside;
}

void
PopInputCapture(void)
{
}

void
ClearInputCaptures(void)
{
}

void
ConsumeRelease(void)
{
    pointer_release_consumed = 1;
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
app_switch_screen(InnerBreeze*app, int screen)
{
    if(app != NULL)
        app->breathing.screen = screen;
}

AppRoute
app_current_route(const InnerBreeze*app)
{
    AppRoute route;

    memset(&route, 0, sizeof(route));
    if(app == NULL)
        return route;
    route.screen = app->breathing.screen;
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
app_switch_route(InnerBreeze*app, AppRoute route)
{
    if(app == NULL)
        return;
    app->breathing.screen = route.screen;
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
app_request_route(InnerBreeze*app, AppRoute route)
{
    route_request_count++;
    app_switch_route(app, route);
}

void
app_leave_practice_config(InnerBreeze*app)
{
    if(app != NULL)
        app->practice_tab = PRACTICE_TAB_PLAY;
}

void
save_settings(InnerBreeze*app)
{
    (void)app;
    save_settings_count++;
}

void
reset_settings_preview(InnerBreeze*app)
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
sync_account_load(SyncAccount *account)
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
app_block_current_click(InnerBreeze*app)
{
    if(app != NULL)
        app->blocked_input_frame = app->breathing.frame;
}

void
app_open_modal(InnerBreeze*app, UIModalType type)
{
    if(app == NULL)
        return;
    app->modal.active = 1;
    app->modal.type = type;
    app_block_current_click(app);
}

void
app_close_modal(InnerBreeze*app)
{
    if(app == NULL)
        return;
    app->modal.active = 0;
    app->modal.type = UIModalNone;
    app_block_current_click(app);
}

int
profile_social_friends_count(InnerBreeze*app)
{
    (void)app;
    return 0;
}

int
profile_social_pending_count(InnerBreeze*app)
{
    (void)app;
    return 0;
}

#include "../build/kryon/generated/src/app/app_nav.c"

static InnerBreeze
test_app(void)
{
    InnerBreeze app;

    memset(&app, 0, sizeof(app));
    app.breathing.screen = ScreenStart;
    app.breathing.frame = 42;
    app.practice_tab = PRACTICE_TAB_PLAY;
    app.main_tab = APP_MAIN_TAB_PRACTICE;
    app_reset_bottom_nav_routes(&app);
    return app;
}

static void
test_default_bottom_nav_routes_include_elist(void)
{
    InnerBreeze app = test_app();

    reset_state();
    app_draw_bottom_nav(&app);

    expect(bottom_nav_draw_count == 1,
           "default bottom nav should draw");
    expect(bottom_nav_last.count == 4,
           "default bottom nav should have four items");
    expect(bottom_nav_last.items[0].route == APP_NAV_ROUTE_ELIST,
           "default first bottom nav item should be EList");
    expect(bottom_nav_last.items[1].route == APP_NAV_ROUTE_HABITS,
           "default second bottom nav item should be habits");
    expect(bottom_nav_last.items[2].route == APP_NAV_ROUTE_PRACTICE,
           "default third bottom nav item should be practice");
    expect(bottom_nav_last.items[3].route == APP_NAV_ROUTE_SETTINGS,
           "default fourth bottom nav item should be settings");
}

static void
test_same_frame_modal_close_consumes_bottom_nav_click(void)
{
    InnerBreeze app = test_app();

    reset_state();
    app.blocked_input_frame = app.breathing.frame;
    mouse_released = 1;
    bottom_nav_clicked_route = APP_NAV_ROUTE_HABITS;

    app_draw_bottom_nav(&app);

    expect(bottom_nav_draw_count == 1,
           "same-frame modal close must keep bottom nav visible");
    expect(app.breathing.screen == ScreenStart,
           "same-frame modal close must not route");
    expect(reset_settings_preview_count == 0,
           "same-frame modal close must not run settings route side effects");
}

static void
test_unblocked_bottom_nav_click_still_routes(void)
{
    InnerBreeze app = test_app();

    reset_state();
    app.blocked_input_frame = app.breathing.frame - 1;
    mouse_released = 1;
    bottom_nav_clicked_route = APP_NAV_ROUTE_HABITS;

    app_draw_bottom_nav(&app);

    expect(bottom_nav_draw_count == 1,
           "unblocked frame should draw bottom nav");
    expect(app.breathing.screen == ScreenHabits,
           "unblocked bottom nav click should route to habits");
    expect(reset_settings_preview_count == 0,
           "habits route should not reset settings preview");
    expect(app_content_bottom_reserved(&app) == 86,
           "bottom nav should reserve larger touch height");
}

static void
test_edge_bottom_nav_routes_are_applied(void)
{
    InnerBreeze app = test_app();

    reset_state();
    bottom_nav_clicked_route = APP_NAV_ROUTE_HABITS;
    app_draw_bottom_nav(&app);

    expect(bottom_nav_draw_count == 1,
           "habits edge route should draw bottom nav");
    expect(app.breathing.screen == ScreenHabits,
           "habits edge route should switch to habits");
}

static void
test_profile_draws_mobile_nav_without_profile_item(void)
{
    InnerBreeze app = test_app();

    reset_state();
    app.breathing.screen = ScreenProfile;

    app_draw_bottom_nav(&app);

    expect(bottom_nav_draw_count == 1,
           "profile should keep mobile bottom nav available");
    expect(bottom_nav_last.count == 4,
           "profile mobile bottom nav should have four items");
    expect(bottom_nav_last.items[0].route == APP_NAV_ROUTE_ELIST,
           "profile mobile nav first item should be EList");
    expect(bottom_nav_last.items[1].route == APP_NAV_ROUTE_HABITS,
           "profile mobile nav second item should be habits");
    expect(bottom_nav_last.items[2].route == APP_NAV_ROUTE_PRACTICE,
           "profile mobile nav third item should be practice");
    expect(bottom_nav_last.items[3].route == APP_NAV_ROUTE_SETTINGS,
           "profile mobile nav fourth item should be settings");
    expect(app_current_nav_route(&app) == APP_NAV_ROUTE_PROFILE,
           "profile should still expose its route for state tracking");
    expect(app_content_bottom_reserved(&app) == 86,
           "profile should reserve mobile bottom nav height");
    expect(app.breathing.screen == ScreenProfile,
           "profile mobile nav should not route without a click");
}

static void
test_practice_manual_hides_bottom_nav(void)
{
    InnerBreeze app = test_app();

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
    expect(app.breathing.screen == ScreenStart,
           "practice manual hidden nav should not route clicks");
}

static void
test_practice_config_hides_bottom_nav(void)
{
    InnerBreeze app = test_app();

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
    expect(app.breathing.screen == ScreenStart,
           "practice config hidden nav should not route clicks");
}

static void
test_file_dialog_hides_bottom_nav(void)
{
    InnerBreeze app = test_app();

    reset_state();
    app.file_dialog_active = 1;
    bottom_nav_clicked_route = APP_NAV_ROUTE_HABITS;

    app_draw_bottom_nav(&app);

    expect(bottom_nav_draw_count == 0,
           "active file dialog should not draw bottom nav");
    expect(app.breathing.screen == ScreenStart,
           "active file dialog should not route bottom nav clicks");
}

static void
test_empty_bottom_nav_recovers_settings_item(void)
{
    InnerBreeze app = test_app();

    reset_state();
    view_width = 720;
    app.main_tab = APP_MAIN_TAB_NONE;
    app.bottom_nav_route_count = 0;
    for(int i = 0; i < APP_BOTTOM_NAV_CONTENT_MAX; i++)
        app.bottom_nav_routes[i] = APP_NAV_ROUTE_NONE;
    bottom_nav_clicked_route = APP_NAV_ROUTE_SETTINGS;

    app_draw_bottom_nav(&app);

    expect(bottom_nav_draw_count == 1,
           "empty bottom nav should recover mandatory settings nav bar");
    expect(bottom_nav_last.count == 1,
           "empty bottom nav should contain only settings");
    expect(bottom_nav_last.items[0].route == APP_NAV_ROUTE_SETTINGS,
           "empty bottom nav only item should be settings");
    expect(app.breathing.screen == ScreenSettings,
           "empty bottom nav settings should open settings");
    expect(app_content_bottom_reserved(&app) == 86,
           "empty bottom nav should reserve settings bar space");
}


static void
test_customize_nav_delete_last_does_not_add_same_frame(void)
{
    InnerBreeze app = test_app();

    reset_state();
    app.breathing.screen = ScreenCustomizeNav;
    app.main_tab = APP_MAIN_TAB_HABITS;
    app.bottom_nav_route_count = 1;
    app.bottom_nav_routes[0] = APP_NAV_ROUTE_HABITS;
    app.bottom_nav_config_route_count = 1;
    app.bottom_nav_config_routes[0] = APP_NAV_ROUTE_HABITS;
    icon_button_click_index = 0;
    generic_button_clicked_label = "customize_nav_add";

    app_draw_customize_nav_page(&app);

    expect(app.bottom_nav_config_route_count == 0,
           "deleting last customize nav row should not also add a row");
    expect(app.bottom_nav_route_count == 1 &&
           app.bottom_nav_routes[0] == APP_NAV_ROUTE_SETTINGS,
           "deleting last customize nav row should keep mandatory settings route");
    expect(save_settings_count == 1,
           "deleting last customize nav row should save exactly once");
    expect(generic_button_clicked_label == NULL,
           "add button click simulation should have been exercised");
}


static void
test_customize_nav_delete_icon_draws_on_narrow_rows(void)
{
    InnerBreeze app = test_app();

    reset_state();
    app.breathing.screen = ScreenCustomizeNav;
    app.bottom_nav_config_route_count = 1;
    app.bottom_nav_config_routes[0] = APP_NAV_ROUTE_HABITS;
    scroll_page_content_w_override = 180;

    app_draw_customize_nav_page(&app);

    expect(icon_button_draw_count == 1,
           "customize nav should draw delete icon on narrow rows");
}

static void
test_bottom_nav_config_save_stays_on_customize_screen(void)
{
    InnerBreeze app = test_app();

    reset_state();
    app.breathing.screen = ScreenCustomizeNav;
    app.main_tab = APP_MAIN_TAB_NONE;
    app.bottom_nav_route_count = 0;
    for(int i = 0; i < APP_BOTTOM_NAV_CONTENT_MAX; i++)
        app.bottom_nav_routes[i] = APP_NAV_ROUTE_NONE;
    app.bottom_nav_config_route_count = 1;
    app.bottom_nav_config_routes[0] = APP_NAV_ROUTE_HABITS;

    app_save_bottom_nav_config(&app);

    expect(app.breathing.screen == ScreenCustomizeNav,
           "adding first nav item should stay on customize nav screen");
    expect(app.main_tab == APP_MAIN_TAB_HABITS,
           "adding first nav item should select a valid main tab");
    expect(app.bottom_nav_route_count == 2 &&
           app.bottom_nav_routes[0] == APP_NAV_ROUTE_HABITS &&
           app.bottom_nav_routes[1] == APP_NAV_ROUTE_SETTINGS,
           "adding first nav item should save configured route plus settings");
    expect(save_settings_count == 1,
           "adding first nav item should save settings once");
}

static void
test_open_main_tab_none_returns_blank_start(void)
{
    InnerBreeze app = test_app();

    reset_state();
    app.breathing.screen = ScreenCustomizeNav;
    app.main_tab = APP_MAIN_TAB_NONE;

    app_open_main_tab(&app, app.main_tab, 0);

    expect(app.breathing.screen == ScreenStart &&
           app.main_tab == APP_MAIN_TAB_NONE,
           "closing customize nav with no routes should return to blank start");
}

static void
test_compact_sidebar_close_footer_closes_to_home(void)
{
    InnerBreeze app = test_app();

    reset_state();
    view_width = 320;
    app.breathing.screen = ScreenNavSidebar;
    app.nav_sidebar_open = 1;
    app.nav_sidebar_open_frame = app.breathing.frame - 1;
    invisible_button_clicked_id = 9199;

    app_draw_bottom_nav(&app);

    expect(app.nav_sidebar_open == 0,
           "sidebar footer close should close sidebar");
    expect(app.breathing.screen == ScreenStart,
           "sidebar footer close should return to current home screen");
    expect(app.blocked_input_frame == app.breathing.frame,
           "sidebar footer close should block same-frame bottom nav clicks");
}

static void
test_overlay_sidebar_outside_release_blocks_bottom_nav(void)
{
    InnerBreeze app = test_app();

    reset_state();
    view_width = 720;
    app.nav_sidebar_open = 1;
    app.nav_sidebar_open_frame = app.breathing.frame - 1;
    mouse_released = 1;
    mouse_position = (Vector2){520, 20};

    app_draw_bottom_nav(&app);

    expect(app.nav_sidebar_open == 0,
           "outside release should close overlay sidebar");
    expect(pointer_release_consumed == 1,
           "outside release should be consumed");
    expect(app.blocked_input_frame == app.breathing.frame,
           "outside release close should block same-frame bottom nav clicks");
}

static void
test_consumed_pfp_release_does_not_close_sidebar(void)
{
    InnerBreeze app = test_app();

    reset_state();
    view_width = 720;
    app.nav_sidebar_open = 1;
    app.nav_sidebar_open_frame = app.breathing.frame - 1;
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
    InnerBreeze app = test_app();

    reset_state();
    view_width = 320;
    app.breathing.screen = ScreenNavSidebar;
    app.nav_sidebar_open = 1;

    app_apply_nav_route(&app, APP_NAV_ROUTE_ACCOUNT);
    expect(app.breathing.screen == ScreenProfile,
           "sidebar account route should open profile");
    expect(app.nav_sidebar_return_on_back == 1,
           "sidebar child route should remember compact sidebar return");

    expect(app_return_to_nav_sidebar_if_needed(&app) == 1,
           "profile back should return to compact sidebar");
    expect(app.breathing.screen == ScreenNavSidebar &&
           app.nav_sidebar_open == 1,
           "profile back should reopen sidebar screen");
}

static void
test_sidebar_screen_closes_when_width_expands(void)
{
    InnerBreeze app = test_app();

    reset_state();
    view_width = 320;
    app.breathing.screen = ScreenNavSidebar;
    app.nav_sidebar_open = 1;

    view_width = 720;
    app_update_nav_sidebar_mode(&app);

    expect(app.nav_sidebar_open == 0,
           "expanded sidebar should close instead of becoming overlay");
    expect(app.breathing.screen == ScreenStart,
           "expanded sidebar should return to home screen");
}

static void
test_habit_editor_preserves_desktop_rail(void)
{
    InnerBreeze app = {0};

    reset_state();
    desktop_mode = 1;
    app.breathing.screen = ScreenHabits;
    expect(app_nav_desktop_rail_enabled(&app), "habits should show desktop rail");
    app.breathing.screen = ScreenHabitEdit;
    app.habit_edit.active = 1;
    for(int is_new = 0; is_new <= 1; is_new++) {
        app.habit_edit.is_new = is_new;
        expect(app_nav_desktop_rail_enabled(&app),
               "new and edit habit should retain desktop rail");
        expect(app_content_bottom_reserved(&app) == 0,
               "desktop habit editor should not reserve mobile navigation");
    }
    desktop_mode = 0;
    expect(!app_nav_desktop_rail_enabled(&app),
           "mobile habit editor must not gain a desktop rail");
    desktop_mode = 1;
    app.file_dialog_active = 1;
    expect(!app_nav_desktop_rail_enabled(&app),
           "file dialog should still hide desktop rail");
    app.file_dialog_active = 0;
    app.breathing.screen = ScreenHabitSessionEdit;
    expect(!app_nav_desktop_rail_enabled(&app),
           "session editor should retain its fullscreen behavior");
    reset_state();
}

static void
test_top_level_nav_leaves_habit_child_screens(void)
{
    InnerBreeze app = test_app();

    reset_state();
    app.breathing.screen = ScreenHabitEdit;
    app.main_tab = APP_MAIN_TAB_HABITS;
    app.habit_edit.active = 1;
    app.habit_edit.index = 1;
    app.habits.screen_mode = HABITS_SCREEN_STATISTICS;
    app.habits.selected = 1;
    app.habits.tab = HABIT_TAB_EDIT;
    app.habits.view_mode = HABIT_VIEW_CALENDAR;
    app.habit_detail_index = 1;
    snprintf(app.habit_detail_session_path, sizeof(app.habit_detail_session_path), "session");

    app_apply_nav_route(&app, APP_NAV_ROUTE_ELIST);

    expect(app.breathing.screen == ScreenEList && app.main_tab == APP_MAIN_TAB_ELIST,
           "top-level EList route should leave habit edit");
    expect(!app.habit_edit.active && app.habit_edit.index == -1,
           "top-level route should clear habit edit state");
    expect(app.habits.screen_mode == HABITS_SCREEN_OVERVIEW &&
           app.habits.selected == -1 &&
           app.habits.tab == HABIT_TAB_WEEKLY &&
           app.habits.view_mode == HABIT_VIEW_WEEKLY,
           "top-level route should reset habit subroute state");
    expect(app.habit_detail_index == -1 && app.habit_detail_session_path[0] == '\0',
           "top-level route should clear habit detail session state");

    app = test_app();
    app.breathing.screen = ScreenHabitSessionEdit;
    app.main_tab = APP_MAIN_TAB_HABITS;
    app.habit_session_edit.active = 1;
    app.habit_session_edit.round = 2;
    app.habits.selected = 0;

    app_apply_nav_route(&app, APP_NAV_ROUTE_PRACTICE);

    expect(app.breathing.screen == ScreenStart && app.main_tab == APP_MAIN_TAB_PRACTICE,
           "top-level Practice route should leave habit session edit");
    expect(!app.habit_session_edit.active && app.habit_session_edit.round == -1,
           "top-level route should clear habit session edit state");
    expect(app.habits.selected == -1 && app.habits.screen_mode == HABITS_SCREEN_OVERVIEW,
           "top-level route should close expanded habit state");
}

static void
test_elist_navigation_sanitizer_and_desktop_route(void)
{
    InnerBreeze app = test_app();

    app.bottom_nav_routes[0] = APP_NAV_ROUTE_HABITS;
    app.bottom_nav_routes[1] = APP_NAV_ROUTE_PRACTICE;
    app.bottom_nav_routes[2] = APP_NAV_ROUTE_SETTINGS;
    app.bottom_nav_route_count = 3;
    app_sanitize_bottom_nav_routes(&app);
    expect(app.bottom_nav_route_count == 3 &&
           app.bottom_nav_routes[0] == APP_NAV_ROUTE_HABITS &&
           app.bottom_nav_routes[1] == APP_NAV_ROUTE_PRACTICE &&
           app.bottom_nav_routes[2] == APP_NAV_ROUTE_SETTINGS,
           "old navigation order should stay as configured");

    app.bottom_nav_routes[0] = APP_NAV_ROUTE_SETTINGS;
    app.bottom_nav_routes[1] = APP_NAV_ROUTE_ELIST;
    app.bottom_nav_routes[2] = APP_NAV_ROUTE_HABITS;
    app.bottom_nav_route_count = 3;
    app_sanitize_bottom_nav_routes(&app);
    expect(app.bottom_nav_routes[0] == APP_NAV_ROUTE_SETTINGS &&
           app.bottom_nav_routes[1] == APP_NAV_ROUTE_ELIST &&
           app.bottom_nav_routes[2] == APP_NAV_ROUTE_HABITS,
           "sanitizer should preserve custom navigation orders");

    app.bottom_nav_routes[0] = APP_NAV_ROUTE_HABITS;
    app.bottom_nav_routes[1] = APP_NAV_ROUTE_PROFILE;
    app.bottom_nav_routes[2] = APP_NAV_ROUTE_SETTINGS;
    app.bottom_nav_route_count = 3;
    app_sanitize_bottom_nav_routes(&app);
    expect(app.bottom_nav_route_count == 2 &&
           app.bottom_nav_routes[0] == APP_NAV_ROUTE_HABITS &&
           app.bottom_nav_routes[1] == APP_NAV_ROUTE_SETTINGS,
           "mobile sanitizer should remove unsupported profile route");

    app = test_app();
    reset_state();
    desktop_mode = 1;
    app_draw_bottom_nav(&app);
    expect(elist_label_count == 1, "desktop rail should display EList");
    mouse_position = (Vector2){-100, 206};
    mouse_released = 1;
    app_draw_bottom_nav(&app);
    expect(app.main_tab == APP_MAIN_TAB_ELIST && app.breathing.screen == ScreenEList,
           "first desktop rail item should open EList");
    reset_state();
}

static void
test_desktop_rail_clicks_route_once_per_release(void)
{
    InnerBreeze app = test_app();

    reset_state();
    desktop_mode = 1;
    view_width = 812;
    view_height = 720;
    mouse_position = (Vector2){-100, 206};
    mouse_released = 1;

    app_draw_bottom_nav(&app);

    expect(route_request_count == 1,
           "desktop rail click should request exactly one route");
    expect(pointer_release_consumed == 1,
           "desktop rail click should consume the release");
    expect(app.main_tab == APP_MAIN_TAB_ELIST &&
           app.breathing.screen == ScreenEList,
           "desktop rail click should route on the same pass");

    app_draw_bottom_nav(&app);

    expect(route_request_count == 1,
           "second nav evaluation in the same frame must not route again");
    expect(app.main_tab == APP_MAIN_TAB_ELIST &&
           app.breathing.screen == ScreenEList,
           "second nav evaluation should preserve the first route");

    reset_state();
    app = test_app();
    desktop_mode = 1;
    view_width = 812;
    view_height = 720;
    app.blocked_input_frame = app.breathing.frame;
    mouse_position = (Vector2){-100, 206};
    mouse_released = 1;

    app_draw_bottom_nav(&app);

    expect(route_request_count == 0,
           "blocked same-frame desktop rail release must not route");
    expect(app.breathing.screen == ScreenStart &&
           app.main_tab == APP_MAIN_TAB_PRACTICE,
           "blocked desktop rail release must leave the screen alone");

    reset_state();
    app = test_app();
    desktop_mode = 1;
    view_width = 812;
    view_height = 720;
    app.nav_rail_collapsed = 1;
    mouse_position = (Vector2){-44, 206};
    mouse_released = 1;

    app_draw_bottom_nav(&app);

    expect(route_request_count == 1,
           "collapsed desktop rail click should request a route");
    expect(app.main_tab == APP_MAIN_TAB_ELIST &&
           app.breathing.screen == ScreenEList,
           "collapsed desktop rail click should route immediately");

    reset_state();
    app = test_app();
    desktop_mode = 1;
    view_width = 812;
    view_height = 720;
    app.navigation_placement = NAVIGATION_RIGHT;
    mouse_position = (Vector2){856, 206};
    mouse_released = 1;

    app_draw_bottom_nav(&app);

    expect(route_request_count == 1,
           "right desktop rail click should request a route");
    expect(app.main_tab == APP_MAIN_TAB_ELIST &&
           app.breathing.screen == ScreenEList,
           "right desktop rail click should route immediately");
    reset_state();
}

static void
test_desktop_rail_spacing(void)
{
    const int heights[] = {476, 560, 720};
    for(int i = 0; i < 3; i++) {
        reset_state();
        InnerBreeze app = test_app();
        desktop_mode = 1;
        view_height = heights[i];
        app_draw_bottom_nav(&app);
        expect(rail_bounds_count == 5, "desktop rail should show profile and four routes");
        for(int j = 0; j < rail_bounds_count; j++) {
            Rectangle bounds = rail_bounds[j];
            expect(bounds.x == -208 && bounds.width == 192,
                   "desktop rail should preserve sixteen-unit side padding");
            expect(bounds.height >= 52 && bounds.y + bounds.height <= view_height - 24,
                   "desktop rail controls must fit with bottom clearance");
            if(j > 0) {
                Rectangle prior = rail_bounds[j - 1];
                expect(bounds.y - prior.y - prior.height >= 12,
                       "desktop rail controls must not touch or overlap");
            }
        }
    }
    reset_state();
    InnerBreeze app = test_app();
    desktop_mode = 1;
    view_height = 360;
    expect(!app_nav_desktop_rail_enabled(&app),
           "short windows must use compact navigation instead of clipping the sidebar");
    reset_state();
}

static void
test_navigation_placement_and_collapse(void)
{
    reset_state();
    InnerBreeze app = test_app();
    expect(app_navigation_placement(&app) == NAVIGATION_BOTTOM,
           "automatic compact navigation belongs at the bottom");
    desktop_mode = 1;
    expect(app_navigation_placement(&app) == NAVIGATION_LEFT,
           "automatic desktop navigation belongs on the left");
    app.navigation_placement = NAVIGATION_RIGHT;
    expect(app_nav_desktop_rail_enabled(&app), "right placement should enable the side rail");
    app_draw_bottom_nav(&app);
    expect(rail_bounds[0].x == view_width + 16,
           "right rail must be outside the content on its right edge");
    invisible_button_clicked_id = 6691;
    app_draw_bottom_nav(&app);
    expect(app.nav_rail_collapsed && app_nav_desktop_rail_width(&app) == 88,
           "collapse control must narrow the rail");
    expect(save_settings_count == 1, "collapsed state must be saved");
    app.breathing.frame++;
    invisible_button_clicked_id = 6691;
    app_draw_bottom_nav(&app);
    expect(!app.nav_rail_collapsed && app_nav_desktop_rail_width(&app) == 224,
           "expand control must restore the full rail");
    app.navigation_placement = NAVIGATION_TOP;
    expect(!app_nav_desktop_rail_enabled(&app), "top placement must not reserve a side rail");
    expect(app_content_bottom_reserved(&app) == 0,
           "top placement must not leave an empty bottom navigation strip");
    app_draw_bottom_nav(&app);
    expect(bottom_nav_last.view_height == 1,
           "top bar must be drawn above the translated content viewport");
    reset_state();
}

int
main(void)
{
    test_desktop_rail_spacing();
    test_navigation_placement_and_collapse();
    test_default_bottom_nav_routes_include_elist();
    test_same_frame_modal_close_consumes_bottom_nav_click();
    test_unblocked_bottom_nav_click_still_routes();
    test_edge_bottom_nav_routes_are_applied();
    test_profile_draws_mobile_nav_without_profile_item();
    test_practice_manual_hides_bottom_nav();
    test_practice_config_hides_bottom_nav();
    test_file_dialog_hides_bottom_nav();
    test_empty_bottom_nav_recovers_settings_item();
    test_bottom_nav_config_save_stays_on_customize_screen();
    test_customize_nav_delete_last_does_not_add_same_frame();
    test_customize_nav_delete_icon_draws_on_narrow_rows();
    test_open_main_tab_none_returns_blank_start();
    test_compact_sidebar_close_footer_closes_to_home();
    test_overlay_sidebar_outside_release_blocks_bottom_nav();
    test_consumed_pfp_release_does_not_close_sidebar();
    test_sidebar_child_back_returns_to_compact_sidebar();
    test_sidebar_screen_closes_when_width_expands();
    test_habit_editor_preserves_desktop_rail();
    test_top_level_nav_leaves_habit_child_screens();
    test_elist_navigation_sanitizer_and_desktop_route();
    test_desktop_rail_clicks_route_once_per_release();

    if(failures > 0) {
        fprintf(stderr, "%d app bottom nav test failure(s)\n", failures);
        return 1;
    }
    printf("app bottom nav tests passed\n");
    return 0;
}
