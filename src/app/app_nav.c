#include "app_nav.h"

#include "app.h"
#include "locale.h"
#include "storage.h"
#include "sync_account.h"
#include "theme.h"
#include "ui.h"
#include "screens/profile_social.h"
#include "screens/settings/settings_screen.h"

#include <stdio.h>

extern int view_width;
extern int view_height;

static int android_bottom_nav_height = 0;

enum {
    APP_BOTTOM_NAV_CONFIGURABLE_COUNT = 2,
};

static void
app_block_nav_click_frame(InbeApp *app)
{
    if(app != NULL)
        app->modal_input_block_frame = app->inbe.frame;
}

static int
app_nav_sidebar_width(void)
{
    int sidebar_w;

    if(view_width <= ScaleUIPx(480))
        return view_width;
    sidebar_w = ScaleUIPx(292);
    if(sidebar_w > view_width - ScaleUIPx(36))
        sidebar_w = view_width - ScaleUIPx(36);
    if(sidebar_w < ScaleUIPx(220))
        sidebar_w = view_width;
    return sidebar_w;
}

static int
app_bottom_nav_configurable_route(int route)
{
    return route == APP_NAV_ROUTE_HABITS ||
           route == APP_NAV_ROUTE_PRACTICE;
}

void
app_reset_bottom_nav_routes(InbeApp *app)
{
    if(app == NULL)
        return;
    app->bottom_nav_routes[0] = APP_NAV_ROUTE_HABITS;
    app->bottom_nav_routes[1] = APP_NAV_ROUTE_PRACTICE;
    for(int i = 2; i < APP_BOTTOM_NAV_CONTENT_MAX; i++)
        app->bottom_nav_routes[i] = APP_NAV_ROUTE_NONE;
    app->bottom_nav_route_count = 2;
}

void
app_sanitize_bottom_nav_routes(InbeApp *app)
{
    int clean[APP_BOTTOM_NAV_CONTENT_MAX];
    int clean_count = 0;

    if(app == NULL)
        return;
    if(app->bottom_nav_route_count < 0 ||
       app->bottom_nav_route_count > APP_BOTTOM_NAV_CONTENT_MAX) {
        app_reset_bottom_nav_routes(app);
        return;
    }
    for(int i = 0; i < app->bottom_nav_route_count; i++) {
        int route = app->bottom_nav_routes[i];
        int duplicate = 0;

        if(!app_bottom_nav_configurable_route(route))
            continue;
        for(int j = 0; j < clean_count; j++) {
            if(clean[j] == route) {
                duplicate = 1;
                break;
            }
        }
        if(duplicate)
            continue;
        clean[clean_count++] = route;
    }
    for(int i = 0; i < clean_count; i++)
        app->bottom_nav_routes[i] = clean[i];
    app->bottom_nav_route_count = clean_count;
}

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
    if(app->inbe.screen == InbeScreenHabits &&
       app->habits.screen_mode == HABITS_SCREEN_REORDER) {
        app->habits.screen_mode = HABITS_SCREEN_OVERVIEW;
        app->habits.scroll = 0;
    }

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
    AppRoute app_route;

    if(app == NULL)
        return;

    if(app->inbe.screen == InbeScreenHabits &&
       app->habits.screen_mode == HABITS_SCREEN_REORDER) {
        app->habits.screen_mode = HABITS_SCREEN_OVERVIEW;
        app->habits.scroll = 0;
    }

    switch(route) {
    case APP_NAV_ROUTE_PROFILE:
    case APP_NAV_ROUTE_ACCOUNT:
        app->nav_sidebar_open = 0;
        if(app->practice_tab == PRACTICE_TAB_CONFIG)
            app_leave_practice_config(app);
        app->profile_scroll = 0;
        app->sync_server_url_focused = 0;
        settings_screen_clear_status();
        app_block_nav_click_frame(app);
        app_route = app_current_route(app);
        app_route.screen = InbeScreenProfile;
        app_route.profile_view = PROFILE_VIEW_SYNC_ACCOUNT;
        app_route.profile_tab = PROFILE_TAB_OVERVIEW;
        app_switch_route(app, app_route);
        break;
    case APP_NAV_ROUTE_DATA:
        app->nav_sidebar_open = 0;
        if(app->practice_tab == PRACTICE_TAB_CONFIG)
            app_leave_practice_config(app);
        app->profile_scroll = 0;
        app->sync_server_url_focused = 0;
        settings_screen_clear_status();
        app_block_nav_click_frame(app);
        app_route = app_current_route(app);
        app_route.screen = InbeScreenProfile;
        app_route.profile_view = PROFILE_VIEW_DATA;
        app_route.profile_tab = PROFILE_TAB_OVERVIEW;
        app_switch_route(app, app_route);
        break;
    case APP_NAV_ROUTE_FRIENDS:
        app->nav_sidebar_open = 0;
        if(app->practice_tab == PRACTICE_TAB_CONFIG)
            app_leave_practice_config(app);
        app->profile_scroll = 0;
        settings_screen_clear_status();
        app_block_nav_click_frame(app);
        app_route = app_current_route(app);
        app_route.screen = InbeScreenProfile;
        app_route.profile_view = PROFILE_VIEW_MAIN;
        app_route.profile_tab = PROFILE_TAB_FRIENDS;
        app_switch_route(app, app_route);
        break;
    case APP_NAV_ROUTE_LEADERBOARD:
        app->nav_sidebar_open = 0;
        if(app->practice_tab == PRACTICE_TAB_CONFIG)
            app_leave_practice_config(app);
        app->profile_scroll = 0;
        settings_screen_clear_status();
        app_block_nav_click_frame(app);
        app_route = app_current_route(app);
        app_route.screen = InbeScreenProfile;
        app_route.profile_view = PROFILE_VIEW_MAIN;
        app_route.profile_tab = PROFILE_TAB_LEADERBOARD;
        app_switch_route(app, app_route);
        break;
    case APP_NAV_ROUTE_PRACTICE:
        app->nav_sidebar_open = 0;
        app_open_main_tab(app, APP_MAIN_TAB_PRACTICE, 1);
        break;
    case APP_NAV_ROUTE_HABITS:
        app->nav_sidebar_open = 0;
        app_open_main_tab(app, APP_MAIN_TAB_HABITS, 1);
        break;
    case APP_NAV_ROUTE_STACK:
        if(!app->modal.active) {
            app->nav_sidebar_open = !app->nav_sidebar_open;
            if(app->nav_sidebar_open)
                app->nav_sidebar_open_frame = app->inbe.frame;
        }
        break;
    default:
        break;
    }
}

static int
app_is_practice_fullscreen_subview(const InbeApp *app)
{
    return app != NULL &&
           app->inbe.screen == InbeScreenStart &&
           (app->practice_tab == PRACTICE_TAB_MANUAL ||
            app->practice_tab == PRACTICE_TAB_CONFIG);
}

int
app_content_bottom_reserved(const InbeApp *app)
{
    if(app == NULL)
        return 0;
    if(app->inbe.screen == InbeScreenHabits &&
       app->habits.screen_mode == HABITS_SCREEN_REORDER)
        return 0;
    if(app_is_practice_fullscreen_subview(app))
        return 0;
    if(app_current_nav_route(app) == APP_NAV_ROUTE_NONE)
        return 0;
    return GetUIBottomNavHeight();
}

int
app_page_height(const InbeApp *app, int full_height)
{
    int bottom_reserved;

    if(full_height <= 0)
        return 0;
    bottom_reserved = app_fullscreen_bottom_reserved(app);
    if(bottom_reserved > 0 && bottom_reserved < full_height)
        return full_height - bottom_reserved;
    return full_height;
}

static int
app_has_fullscreen_overlay(const InbeApp *app)
{
    if(app == NULL || !app->modal.active)
        return 0;
    return app->modal.type == UIModalEditProgressiveStartSpeed;
}

static const char *
app_nav_route_label(int route)
{
    switch(route) {
    case APP_NAV_ROUTE_PROFILE:
    case APP_NAV_ROUTE_ACCOUNT: return GetLocaleText("sync_configure_account_button");
    case APP_NAV_ROUTE_DATA: return GetLocaleText("profile_data_title");
    case APP_NAV_ROUTE_FRIENDS: return GetLocaleText("profile_friends_title");
    case APP_NAV_ROUTE_LEADERBOARD: return GetLocaleText("profile_leaderboard_title");
    case APP_NAV_ROUTE_HABITS: return GetLocaleText("tab_habits");
    case APP_NAV_ROUTE_PRACTICE: return GetLocaleText("tab_practice");
    case APP_NAV_ROUTE_STACK: return GetLocaleText("tab_stack");
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
    case APP_NAV_ROUTE_PROFILE:
    case APP_NAV_ROUTE_ACCOUNT: return app->icons[UI_ICON_TYPE_PROFILE];
    case APP_NAV_ROUTE_DATA: return app->icons[UI_ICON_TYPE_SAVE];
    case APP_NAV_ROUTE_FRIENDS: return app->icons[UI_ICON_TYPE_PROFILE];
    case APP_NAV_ROUTE_LEADERBOARD: return app->icons[UI_ICON_TYPE_STAT];
    case APP_NAV_ROUTE_HABITS: return app->icons[UI_ICON_TYPE_HABIT];
    case APP_NAV_ROUTE_PRACTICE: return app->icons[UI_ICON_TYPE_AMEN];
    case APP_NAV_ROUTE_STACK: return app->icons[UI_ICON_TYPE_STACK];
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
        if(app->profile_view == PROFILE_VIEW_SYNC_ACCOUNT)
            return APP_NAV_ROUTE_ACCOUNT;
        if(app->profile_view == PROFILE_VIEW_DATA ||
           app->profile_tab == PROFILE_TAB_DATA)
            return APP_NAV_ROUTE_DATA;
        if(app->profile_tab == PROFILE_TAB_FRIENDS)
            return APP_NAV_ROUTE_FRIENDS;
        if(app->profile_tab == PROFILE_TAB_LEADERBOARD)
            return APP_NAV_ROUTE_LEADERBOARD;
        return APP_NAV_ROUTE_ACCOUNT;
    case InbeScreenStart:
        if(app_is_practice_fullscreen_subview(app))
            return APP_NAV_ROUTE_NONE;
        return APP_NAV_ROUTE_PRACTICE;
    case InbeScreenHabits:
        return APP_NAV_ROUTE_HABITS;
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
    if(route == APP_NAV_ROUTE_ACCOUNT)
        return app->inbe.screen == InbeScreenProfile &&
               app->profile_view == PROFILE_VIEW_SYNC_ACCOUNT;
    if(route == APP_NAV_ROUTE_DATA)
        return app->inbe.screen == InbeScreenProfile &&
               (app->profile_view == PROFILE_VIEW_DATA ||
                app->profile_tab == PROFILE_TAB_DATA);
    if(route == APP_NAV_ROUTE_FRIENDS)
        return app->inbe.screen == InbeScreenProfile &&
               app->profile_tab == PROFILE_TAB_FRIENDS;
    if(route == APP_NAV_ROUTE_LEADERBOARD)
        return app->inbe.screen == InbeScreenProfile &&
               app->profile_tab == PROFILE_TAB_LEADERBOARD;
    if(route == APP_NAV_ROUTE_PRACTICE)
        return app->inbe.screen == InbeScreenStart;
    if(route == APP_NAV_ROUTE_HABITS)
        return app->inbe.screen == InbeScreenHabits ||
               app->inbe.screen == InbeScreenHabitEdit ||
               app->inbe.screen == InbeScreenHabitSessionEdit;
    if(route == APP_NAV_ROUTE_STACK)
        return app->nav_sidebar_open ||
               (app->modal.active && app->modal.type == UIModalBottomNavConfig);
    return 0;
}

static int
app_should_draw_bottom_nav(const InbeApp *app)
{
    if(app == NULL)
        return 0;
    if(app->file_dialog_active)
        return 0;
    if(app_has_fullscreen_overlay(app))
        return 0;
    if(app->inbe.screen == InbeScreenHabits &&
       app->habits.screen_mode == HABITS_SCREEN_REORDER)
        return 0;
    return app_current_nav_route(app) != APP_NAV_ROUTE_NONE;
}

int
app_fullscreen_bottom_reserved(const InbeApp *app)
{
    if(app == NULL)
        return 0;
    return app_android_bottom_nav_height();
}

static int
app_sidebar_button(InbeApp *app, int x, int *y, int w, const char *label,
                   int route)
{
    int hover = 0;
    int h = ScaleUIPx(38);
    int disabled = app != NULL && app->modal.active;

    if(DrawUIGenericButton(x, *y, w, h, label, UI_BUTTON_STYLE_SECONDARY,
                              disabled ||
                                  route == APP_NAV_ROUTE_NONE,
                              &hover)) {
        app_apply_nav_route(app, route);
        return 1;
    }
    *y += h + ScaleUIPx(8);
    return 0;
}

static void
app_sidebar_account_label(char *out, size_t out_size)
{
    InbeSyncAccount account;
    const char *alias;

    if(out == NULL || out_size == 0)
        return;
    if(!sync_account_load(&account)) {
        snprintf(out, out_size, "%s", GetLocaleText("sync_configure_account_button"));
        return;
    }
    alias = storage_get_setting_text("sync_account_alias");
    if(alias != NULL && alias[0] != '\0')
        snprintf(out, out_size, "@%s", alias);
    else
        snprintf(out, out_size, "%.12s...", account.public_id);
}

static int
app_draw_sidebar_profile_header(InbeApp *app, int x, int *y, int w,
                                Vector2 mouse, int has_account,
                                const char *account_label)
{
    int header_h = ScaleUIPx(138);
    int banner_h = ScaleUIPx(48);
    int avatar_r = ScaleUIPx(28);
    int avatar_x = x + ScaleUIPx(16) + avatar_r;
    int avatar_y = *y + banner_h + ScaleUIPx(24);
    int name_x = avatar_x + avatar_r + ScaleUIPx(14);
    int name_y = avatar_y - ScaleUIPx(14);
    int name_font = GetUIFontSize();
    int small = GetUISmallFontSize();
    int count_y = avatar_y + avatar_r + ScaleUIPx(8);
    int half_w = (w - ScaleUIPx(8)) / 2;
    int released = app != NULL &&
                   app->nav_sidebar_open_frame != app->inbe.frame &&
                   IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    Rectangle name_bounds = {(float)x,
                             (float)(*y + banner_h - ScaleUIPx(8)),
                             (float)w, (float)ScaleUIPx(80)};
    Rectangle friends_bounds = {(float)x, (float)count_y, (float)half_w,
                                (float)ScaleUIPx(42)};
    Rectangle pending_bounds = {(float)(x + half_w + ScaleUIPx(8)),
                                (float)count_y, (float)half_w,
                                (float)ScaleUIPx(42)};
    char friends_text[64];
    char pending_text[64];

    snprintf(friends_text, sizeof(friends_text),
             GetLocaleText("profile_friends_count_format"),
             has_account ? profile_social_friends_count(app) : 0);
    snprintf(pending_text, sizeof(pending_text),
             GetLocaleText("profile_pending_count_format"),
             has_account ? profile_social_pending_count(app) : 0);

    DrawRectangleRounded((Rectangle){(float)x, (float)*y, (float)w,
                                     (float)header_h},
                         0.06f, 8, DarkenUIColor(GetThemeSurface(), 6));
    DrawRectangleRounded((Rectangle){(float)x, (float)*y, (float)w,
                                     (float)banner_h},
                         0.06f, 8, DarkenUIColor(GetThemeButton(), 12));
    DrawRectangle(x, *y + banner_h - ScaleUIPx(12), w, ScaleUIPx(12),
                  DarkenUIColor(GetThemeButton(), 12));
    DrawCircle(avatar_x, avatar_y, (float)(avatar_r + ScaleUIPx(3)),
               GetThemeSurface());
    DrawCircle(avatar_x, avatar_y, (float)avatar_r,
               LightenUIColor(GetThemeSurface(), 12));
    DrawCircleLines(avatar_x, avatar_y, (float)avatar_r,
                    DarkenUIColor(GetThemeText(), 38));
    if(app != NULL && app->icons[UI_ICON_TYPE_PROFILE].id != 0) {
        int icon_size = avatar_r + ScaleUIPx(10);

        DrawUIIconTexture(avatar_x - icon_size / 2, avatar_y - icon_size / 2,
                          icon_size, app->icons[UI_ICON_TYPE_PROFILE],
                          GetThemeIcon());
    }

    DrawUIText(account_label, name_x, name_y, name_font, GetThemeText());
    if(!has_account)
        DrawUIText(GetLocaleText("profile_no_account"), name_x,
                   name_y + ScaleUIPx(22), small,
                   DarkenUIColor(GetThemeText(), 34));

    DrawUIText(friends_text, x + ScaleUIPx(12), count_y + ScaleUIPx(8),
               small, GetThemeText());
    DrawUIText(pending_text, x + half_w + ScaleUIPx(20),
               count_y + ScaleUIPx(8), small, GetThemeText());
    DrawLine(x + half_w + ScaleUIPx(4), count_y + ScaleUIPx(6),
             x + half_w + ScaleUIPx(4), count_y + ScaleUIPx(30),
             DarkenUIColor(GetThemeText(), 48));

    if(released &&
       (CheckCollisionPointRec(mouse, friends_bounds) ||
        CheckCollisionPointRec(mouse, pending_bounds))) {
        app_apply_nav_route(app, has_account ? APP_NAV_ROUTE_FRIENDS
                                             : APP_NAV_ROUTE_ACCOUNT);
        *y += header_h + ScaleUIPx(12);
        return 1;
    } else if(released && CheckCollisionPointRec(mouse, name_bounds)) {
        app_apply_nav_route(app, APP_NAV_ROUTE_ACCOUNT);
        *y += header_h + ScaleUIPx(12);
        return 1;
    }

    *y += header_h + ScaleUIPx(12);
    return 0;
}

static void
app_open_bottom_nav_config(InbeApp *app)
{
    if(app == NULL)
        return;
    app_sanitize_bottom_nav_routes(app);
    for(int i = 0; i < app->bottom_nav_route_count; i++)
        app->bottom_nav_config_routes[i] = app->bottom_nav_routes[i];
    app->bottom_nav_config_route_count = app->bottom_nav_route_count;
    app->nav_sidebar_open = 0;
    app_block_nav_click_frame(app);
    app_switch_screen(app, InbeScreenCustomizeNav);
}

static void
app_draw_nav_sidebar(InbeApp *app)
{
    int sidebar_w;
    int sidebar_x;
    int pad;
    int y;
    int hover = 0;
    Vector2 mouse;
    Color scrim = BLACK;
    UIScrollArea area;
    UIScrollView view;
    InbeSyncAccount account;
    int has_account;
    char account_label[96];

    if(app == NULL || !app->nav_sidebar_open)
        return;

    ClearUIInputCaptures();

    sidebar_w = app_nav_sidebar_width();
    sidebar_x = view_width - sidebar_w;
    pad = ScaleUIPx(16);

    scrim.a = 92;
    DrawRectangle(0, 0, view_width, view_height, scrim);
    DrawRectangle(sidebar_x, 0, sidebar_w, view_height, GetThemeSurface());
    if(sidebar_x > 0)
        DrawLine(sidebar_x, 0, sidebar_x, view_height,
                 DarkenUIColor(GetThemeSurface(), 42));

    if(app->nav_sidebar_open_frame == app->inbe.frame)
        return;

    mouse = GetScreenToWorld2D(GetMousePosition(), app->camera);
    if(app->nav_sidebar_open_frame != app->inbe.frame &&
       IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
       !CheckCollisionPointRec(mouse, (Rectangle){(float)sidebar_x, 0.0f,
                                                  (float)sidebar_w,
                                                  (float)view_height})) {
        app->nav_sidebar_open = 0;
        return;
    }

    area = (UIScrollArea){
        .bounds = {(float)sidebar_x, 0.0f, (float)sidebar_w,
                   (float)(view_height - app_fullscreen_bottom_reserved(app))},
        .content_height = ScaleUIPx(540),
        .content_x = sidebar_x + pad,
        .content_width = sidebar_w - pad * 2,
        .scroll_offset = &app->nav_sidebar_scroll,
        .wheel_step = ScaleUIPx(42),
        .scrollbar_x = sidebar_x + sidebar_w - ScaleUIPx(8)
    };
    view = BeginUIScrollContainer(area);
    y = view.content_y;

    has_account = sync_account_load(&account);
    app_sidebar_account_label(account_label, sizeof(account_label));
    if(app_draw_sidebar_profile_header(app, area.content_x, &y, area.content_width,
                                       mouse, has_account, account_label)) {
        EndUIScrollContainer(area, view);
        return;
    }
    if(app_sidebar_button(app, area.content_x, &y, area.content_width,
                          GetLocaleText("sync_configure_account_button"),
                          APP_NAV_ROUTE_ACCOUNT)) {
        EndUIScrollContainer(area, view);
        return;
    }
    if(app_sidebar_button(app, area.content_x, &y, area.content_width,
                          GetLocaleText("profile_friends_title"),
                          APP_NAV_ROUTE_FRIENDS)) {
        EndUIScrollContainer(area, view);
        return;
    }
    if(app_sidebar_button(app, area.content_x, &y, area.content_width,
                          GetLocaleText("profile_leaderboard_title"),
                          APP_NAV_ROUTE_LEADERBOARD)) {
        EndUIScrollContainer(area, view);
        return;
    }
    y += ScaleUIPx(10);
    if(DrawUIGenericButton(area.content_x, y, area.content_width, ScaleUIPx(40),
                              GetLocaleText("customize_nav_button"),
                              UI_BUTTON_STYLE_PRIMARY, app->modal.active, &hover)) {
        app_open_bottom_nav_config(app);
        EndUIScrollContainer(area, view);
        return;
    }

    EndUIScrollContainer(area, view);
}

static int
app_nav_option_index(int route)
{
    switch(route) {
    case APP_NAV_ROUTE_HABITS: return 0;
    case APP_NAV_ROUTE_PRACTICE: return 1;
    default: break;
    }
    return 0;
}

static int
app_nav_route_for_option(int option)
{
    static const int routes[] = {
        APP_NAV_ROUTE_HABITS,
        APP_NAV_ROUTE_PRACTICE
    };

    if(option < 0 || option >= APP_BOTTOM_NAV_CONFIGURABLE_COUNT)
        return APP_NAV_ROUTE_HABITS;
    return routes[option];
}

static void
app_save_bottom_nav_config(InbeApp *app)
{
    if(app == NULL)
        return;
    app->bottom_nav_route_count = app->bottom_nav_config_route_count;
    for(int i = 0; i < APP_BOTTOM_NAV_CONTENT_MAX; i++)
        app->bottom_nav_routes[i] = i < app->bottom_nav_route_count
                                        ? app->bottom_nav_config_routes[i]
                                        : APP_NAV_ROUTE_NONE;
    app_sanitize_bottom_nav_routes(app);
    save_settings(app);
}

static int
app_move_bottom_nav_config_route(InbeApp *app, int from_index, int to_index)
{
    int route;

    if(app == NULL)
        return 0;
    if(from_index < 0 || to_index < 0 ||
       from_index >= app->bottom_nav_config_route_count ||
       to_index >= app->bottom_nav_config_route_count ||
       from_index == to_index)
        return 0;

    route = app->bottom_nav_config_routes[from_index];
    if(from_index < to_index) {
        for(int i = from_index; i < to_index; i++)
            app->bottom_nav_config_routes[i] =
                app->bottom_nav_config_routes[i + 1];
    } else {
        for(int i = from_index; i > to_index; i--)
            app->bottom_nav_config_routes[i] =
                app->bottom_nav_config_routes[i - 1];
    }
    app->bottom_nav_config_routes[to_index] = route;
    return 1;
}

static int
app_first_unused_bottom_nav_route(const InbeApp *app)
{
    static const int routes[] = {
        APP_NAV_ROUTE_HABITS,
        APP_NAV_ROUTE_PRACTICE
    };

    for(int i = 0; i < (int)(sizeof(routes) / sizeof(routes[0])); i++) {
        int used = 0;

        for(int j = 0; app != NULL && j < app->bottom_nav_config_route_count; j++) {
            if(app->bottom_nav_config_routes[j] == routes[i]) {
                used = 1;
                break;
            }
        }
        if(!used)
            return routes[i];
    }

    return APP_NAV_ROUTE_HABITS;
}

int
app_draw_customize_nav_page(InbeApp *app)
{
    const char *options[APP_BOTTOM_NAV_CONFIGURABLE_COUNT];
    UIScrollPage page;
    UIReorderItem reorder_items[APP_BOTTOM_NAV_CONTENT_MAX];
    UIReorderListResult reorder;
    int selected[APP_BOTTOM_NAV_CONTENT_MAX];
    int row_y[APP_BOTTOM_NAV_CONTENT_MAX];
    int y;
    int changed = 0;
    int button_h = ScaleUIPx(36);
    int row_h = ScaleUIPx(58);
    int handle_w = ScaleUIPx(34);

    if(app == NULL)
        return 0;
    if(app->bottom_nav_config_route_count < 0 ||
       app->bottom_nav_config_route_count > APP_BOTTOM_NAV_CONTENT_MAX) {
        app_sanitize_bottom_nav_routes(app);
        for(int i = 0; i < app->bottom_nav_route_count; i++)
            app->bottom_nav_config_routes[i] = app->bottom_nav_routes[i];
        app->bottom_nav_config_route_count = app->bottom_nav_route_count;
    }

    if(DrawUIReturnTitleBar(app->icons[UI_ICON_TYPE_RETURN],
                            GetLocaleText("customize_nav_title"),
                            GetUITabBarHeight())) {
        app_block_nav_click_frame(app);
        app_open_main_tab(app, app->main_tab, 0);
        return 1;
    }

    options[0] = app_nav_route_label(APP_NAV_ROUTE_HABITS);
    options[1] = app_nav_route_label(APP_NAV_ROUTE_PRACTICE);

    page = BeginUIScrollPage((UIScrollPageSpec){
        .y = GetUITabBarHeight() + ScaleUIPx(8),
        .height = view_height - GetUITabBarHeight() - ScaleUIPx(8),
        .max_content_width = ScaleUIPx(CONTENT_MAX_W),
        .min_content_width = ScaleUIPx(300),
        .scroll_offset = &app->nav_sidebar_scroll
    });
    y = page.content_y;

    for(int i = 0; i < APP_BOTTOM_NAV_CONTENT_MAX; i++)
        selected[i] = app_nav_option_index(app->bottom_nav_config_routes[i]);

    for(int i = 0; i < app->bottom_nav_config_route_count; i++) {
        row_y[i] = y + i * row_h;
        reorder_items[i] = (UIReorderItem){
            .id = app->bottom_nav_config_routes[i] * 100 + i + 1,
            .bounds = {(float)page.content_x, (float)row_y[i],
                       (float)page.content_w, (float)row_h},
            .disabled = 0
        };
    }

    reorder = UpdateUIReorderList((UIReorderList){
        .id = 701,
        .bounds = {(float)page.content_x, (float)page.content_y,
                   (float)page.content_w,
                   (float)(row_h * app->bottom_nav_config_route_count)},
        .items = reorder_items,
        .item_count = app->bottom_nav_config_route_count,
        .handle_width = handle_w,
        .scroll_offset = &app->nav_sidebar_scroll,
        .max_scroll = page.view.max_scroll,
        .viewport_top = (int)page.area.bounds.y,
        .viewport_bottom = (int)(page.area.bounds.y + page.area.bounds.height)
    });
    if(reorder.committed &&
       app_move_bottom_nav_config_route(app, reorder.from_index,
                                        reorder.to_index)) {
        changed = 1;
        app_save_bottom_nav_config(app);
        for(int i = 0; i < APP_BOTTOM_NAV_CONTENT_MAX; i++)
            selected[i] = app_nav_option_index(app->bottom_nav_config_routes[i]);
    }

    for(int i = 0; i < app->bottom_nav_config_route_count; i++) {
        int trash_hover = 0;
        int icon_w = ScaleUIPx(36);
        int dropdown_x = page.content_x + handle_w;
        int dropdown_w = page.content_w - handle_w - icon_w - ScaleUIPx(8);
        int dropdown_y = row_y[i] + ScaleUIPx(18);

        if(reorder.dragging && i == reorder.active_index)
            continue;

        if(dropdown_w < ScaleUIPx(140))
            dropdown_w = page.content_w - handle_w;
        DrawUIReorderHandle(page.content_x, row_y[i], handle_w, row_h,
                            reorder.active && i == reorder.active_index);
        DrawUIText(GetLocaleText("customize_nav_slot"), dropdown_x, row_y[i],
                   GetUISmallFontSize(), DarkenUIColor(GetThemeText(), 34));
        DrawUIDropdownButton(740 + i, dropdown_x, dropdown_y,
                             dropdown_w, button_h, options,
                             APP_BOTTOM_NAV_CONFIGURABLE_COUNT, &selected[i]);
        if(dropdown_x + dropdown_w + ScaleUIPx(8) + icon_w <=
           page.content_x + page.content_w) {
            if(DrawUIPaddedIconBtn(page.content_x + page.content_w - icon_w,
                                   dropdown_y - ScaleUIPx(2), ScaleUIPx(20),
                                   ScaleUIPx(8), app->icons[UI_ICON_TYPE_TRASH],
                                   &trash_hover)) {
                for(int j = i; j < app->bottom_nav_config_route_count - 1; j++)
                    app->bottom_nav_config_routes[j] =
                        app->bottom_nav_config_routes[j + 1];
                app->bottom_nav_config_route_count--;
                changed = 1;
                app_save_bottom_nav_config(app);
                break;
            }
        }
    }
    if(reorder.dragging && reorder.target_index >= 0 &&
       reorder.target_index < app->bottom_nav_config_route_count)
        DrawUIReorderPlaceholder(reorder_items[reorder.target_index].bounds);
    if(reorder.dragging && reorder.active_index >= 0 &&
       reorder.active_index < app->bottom_nav_config_route_count) {
        int i = reorder.active_index;
        int trash_hover = 0;
        int icon_w = ScaleUIPx(36);
        int dropdown_x = page.content_x + handle_w;
        int dropdown_w = page.content_w - handle_w - icon_w - ScaleUIPx(8);
        int overlay_y = row_y[i] + reorder.drag_delta_y;
        int dropdown_y = overlay_y + ScaleUIPx(18);

        if(dropdown_w < ScaleUIPx(140))
            dropdown_w = page.content_w - handle_w;
        DrawRectangle(page.content_x, overlay_y, page.content_w, row_h,
                      LightenUIColor(GetThemeBackground(), 8));
        DrawUIReorderHandle(page.content_x, overlay_y, handle_w, row_h, 1);
        DrawUIText(GetLocaleText("customize_nav_slot"), dropdown_x, overlay_y,
                   GetUISmallFontSize(), DarkenUIColor(GetThemeText(), 34));
        DrawUIDropdownButton(740 + i, dropdown_x, dropdown_y,
                             dropdown_w, button_h, options,
                             APP_BOTTOM_NAV_CONFIGURABLE_COUNT, &selected[i]);
        (void)trash_hover;
        if(dropdown_x + dropdown_w + ScaleUIPx(8) + icon_w <=
           page.content_x + page.content_w)
            DrawUIIconTexture(page.content_x + page.content_w - icon_w +
                              ScaleUIPx(8), dropdown_y + ScaleUIPx(6),
                              ScaleUIPx(20), app->icons[UI_ICON_TYPE_TRASH],
                              GetThemeIcon());
    }

    y += row_h * app->bottom_nav_config_route_count;

    if(app->bottom_nav_config_route_count < APP_BOTTOM_NAV_CONFIGURABLE_COUNT) {
        int add_hover = 0;
        if(DrawUIGenericButton(page.content_x, y, page.content_w, button_h,
                                  GetLocaleText("customize_nav_add"),
                                  UI_BUTTON_STYLE_SECONDARY, 0, &add_hover)) {
            int route = app_first_unused_bottom_nav_route(app);
            app->bottom_nav_config_routes[app->bottom_nav_config_route_count++] = route;
            changed = 1;
            app_save_bottom_nav_config(app);
        }
        y += button_h + ScaleUIPx(14);
    }

    SetUIDropdownClipTop(GetUITabBarHeight() + ScaleUIPx(8));
    SetUIDropdownClipBottom(view_height);
    for(int i = 0; i < app->bottom_nav_config_route_count; i++) {
        if(DrawUIDropdownMenu(740 + i)) {
            int old_route = app->bottom_nav_config_routes[i];
            int new_route = app_nav_route_for_option(selected[i]);

            if(new_route != old_route) {
                for(int j = 0; j < app->bottom_nav_config_route_count; j++) {
                    if(j != i && app->bottom_nav_config_routes[j] == new_route) {
                        app->bottom_nav_config_routes[j] = old_route;
                        break;
                    }
                }
            }
            app->bottom_nav_config_routes[i] = new_route;
            changed = 1;
            app_save_bottom_nav_config(app);
        }
    }
    SetUIDropdownClipTop(0);
    SetUIDropdownClipBottom(0);

    EndUIScrollPage(page);
    (void)changed;
    return 0;
}

void
app_draw_bottom_nav(InbeApp *app)
{
    int routes[APP_BOTTOM_NAV_CONTENT_MAX + 1];
    int route_count;
    UIBottomNavItem items[APP_BOTTOM_NAV_CONTENT_MAX + 1];
    UIBottomNavResult result;

    if(app_android_bottom_nav_height() > 0)
        DrawRectangle(0, view_height - app_android_bottom_nav_height(),
                      view_width, app_android_bottom_nav_height(), BLACK);
    if(app != NULL && app->nav_sidebar_open) {
        app_draw_nav_sidebar(app);
        return;
    }
    if(app_should_draw_bottom_nav(app)) {
        app_sanitize_bottom_nav_routes(app);
        route_count = app->bottom_nav_route_count;
        for(int i = 0; i < route_count; i++)
            routes[i] = app->bottom_nav_routes[i];
        routes[route_count++] = APP_NAV_ROUTE_STACK;

        if(app->modal_input_block_frame != app->inbe.frame ||
           !IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            for(int i = 0; i < route_count; i++) {
                int route = routes[i];
                items[i] = (UIBottomNavItem){
                    route,
                    app_nav_route_label(route),
                    app_nav_route_icon(app, route),
                    app_nav_route_active(app, route),
                    app->modal.active
                };
            }
            result = DrawUIBottomNav((UIBottomNav){
                .view_width = view_width,
                .view_height = view_height,
                .bottom_margin = app_android_bottom_nav_height(),
                .count = route_count,
                .items = items
            });
            if(result.clicked_route != APP_NAV_ROUTE_NONE)
                app_apply_nav_route(app, result.clicked_route);
        }
    }
    app_draw_nav_sidebar(app);
}
