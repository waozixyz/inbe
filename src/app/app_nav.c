#include "app_nav.h"

#include "app.h"
#include "locale.h"
#include "ui.h"
#include "screens/settings/settings_screen.h"

extern int view_width;
extern int view_height;

static int android_bottom_nav_height = 0;

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
    return route == APP_NAV_ROUTE_PROFILE ||
           route == APP_NAV_ROUTE_HABITS ||
           route == APP_NAV_ROUTE_PRACTICE ||
           route == APP_NAV_ROUTE_PET ||
           route == APP_NAV_ROUTE_SETTINGS;
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
    if(app->bottom_nav_route_count < 1 ||
       app->bottom_nav_route_count > APP_BOTTOM_NAV_CONTENT_MAX) {
        app_reset_bottom_nav_routes(app);
        return;
    }
    for(int i = 0; i < app->bottom_nav_route_count; i++) {
        int route = app->bottom_nav_routes[i];
        int duplicate = 0;

        if(!app_bottom_nav_configurable_route(route)) {
            app_reset_bottom_nav_routes(app);
            return;
        }
        for(int j = 0; j < clean_count; j++) {
            if(clean[j] == route) {
                duplicate = 1;
                break;
            }
        }
        if(duplicate) {
            app_reset_bottom_nav_routes(app);
            return;
        }
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

static void
app_open_settings_tab(InbeApp *app, int tab)
{
    if(app == NULL)
        return;
    if(app->practice_tab == PRACTICE_TAB_CONFIG)
        app_leave_practice_config(app);
    reset_settings_preview(app);
    app->settings_tab = tab;
    app->settings_scroll = 0;
    settings_screen_clear_status();
    app_switch_screen(app, InbeScreenSettings);
}

void
app_apply_nav_route(InbeApp *app, int route)
{
    if(app == NULL)
        return;

    if(app->inbe.screen == InbeScreenHabits &&
       app->habits.screen_mode == HABITS_SCREEN_REORDER) {
        app->habits.screen_mode = HABITS_SCREEN_OVERVIEW;
        app->habits.scroll = 0;
    }

    switch(route) {
    case APP_NAV_ROUTE_PROFILE:
        app->nav_sidebar_open = 0;
        if(app->practice_tab == PRACTICE_TAB_CONFIG)
            app_leave_practice_config(app);
        app->profile_view = PROFILE_VIEW_MAIN;
        app->profile_scroll = 0;
        app->sync_server_url_focused = 0;
        settings_screen_clear_status();
        app_switch_screen(app, InbeScreenProfile);
        break;
    case APP_NAV_ROUTE_PRACTICE:
        app->nav_sidebar_open = 0;
        app_open_main_tab(app, APP_MAIN_TAB_PRACTICE, 1);
        break;
    case APP_NAV_ROUTE_HABITS:
        app->nav_sidebar_open = 0;
        app_open_main_tab(app, APP_MAIN_TAB_HABITS, 1);
        break;
    case APP_NAV_ROUTE_SETTINGS:
        app->nav_sidebar_open = 0;
        app_open_settings_tab(app, SETTINGS_TAB_OVERVIEW);
        break;
    case APP_NAV_ROUTE_PET:
        app->nav_sidebar_open = 0;
        if(app->practice_tab == PRACTICE_TAB_CONFIG)
            app_leave_practice_config(app);
        app_switch_screen(app, InbeScreenPet);
        break;
    case APP_NAV_ROUTE_STACK:
        if(!app->modal.active)
            app->nav_sidebar_open = !app->nav_sidebar_open;
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
    case APP_NAV_ROUTE_PROFILE: return GetLocaleText("tab_profile");
    case APP_NAV_ROUTE_HABITS: return GetLocaleText("tab_habits");
    case APP_NAV_ROUTE_PRACTICE: return GetLocaleText("tab_practice");
    case APP_NAV_ROUTE_PET: return GetLocaleText("tab_pet");
    case APP_NAV_ROUTE_SETTINGS: return GetLocaleText("tab_settings");
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
    case APP_NAV_ROUTE_PROFILE: return app->icons[UI_ICON_TYPE_PROFILE];
    case APP_NAV_ROUTE_HABITS: return app->icons[UI_ICON_TYPE_HABIT];
    case APP_NAV_ROUTE_PRACTICE: return app->icons[UI_ICON_TYPE_AMEN];
    case APP_NAV_ROUTE_PET: return app->icons[UI_ICON_TYPE_PET];
    case APP_NAV_ROUTE_SETTINGS: return app->icons[UI_ICON_TYPE_GEAR];
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
        return APP_NAV_ROUTE_PROFILE;
    case InbeScreenStart:
        if(app_is_practice_fullscreen_subview(app))
            return APP_NAV_ROUTE_NONE;
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
                   int route, int settings_tab)
{
    int hover = 0;
    int h = ScaleUIPx(38);

    if(DrawUIGenericButton(x, *y, w, h, label, UI_BUTTON_STYLE_SECONDARY,
                              app != NULL && app->modal.active, &hover)) {
        if(settings_tab >= SETTINGS_TAB_OVERVIEW) {
            app->nav_sidebar_open = 0;
            app_open_settings_tab(app, settings_tab);
        } else {
            app_apply_nav_route(app, route);
        }
        return 1;
    }
    *y += h + ScaleUIPx(8);
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

    if(app == NULL || !app->nav_sidebar_open)
        return;

    sidebar_w = app_nav_sidebar_width();
    sidebar_x = view_width - sidebar_w;
    pad = ScaleUIPx(16);

    scrim.a = 92;
    DrawRectangle(0, 0, view_width, view_height, scrim);
    DrawRectangle(sidebar_x, 0, sidebar_w, view_height, GetThemeSurface());
    if(sidebar_x > 0)
        DrawLine(sidebar_x, 0, sidebar_x, view_height,
                 DarkenUIColor(GetThemeSurface(), 42));

    mouse = GetScreenToWorld2D(GetMousePosition(), app->camera);
    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
       !CheckCollisionPointRec(mouse, (Rectangle){(float)sidebar_x, 0.0f,
                                                  (float)sidebar_w,
                                                  (float)view_height})) {
        app->nav_sidebar_open = 0;
        return;
    }

    area = (UIScrollArea){
        .bounds = {(float)sidebar_x, 0.0f, (float)sidebar_w,
                   (float)(view_height - app_fullscreen_bottom_reserved(app))},
        .content_height = ScaleUIPx(330),
        .content_x = sidebar_x + pad,
        .content_width = sidebar_w - pad * 2,
        .scroll_offset = &app->nav_sidebar_scroll,
        .wheel_step = ScaleUIPx(42),
        .scrollbar_x = sidebar_x + sidebar_w - ScaleUIPx(8)
    };
    view = BeginUIScrollContainer(area);
    y = view.content_y;

    app_sidebar_button(app, area.content_x, &y, area.content_width,
                       GetLocaleText("tab_profile"), APP_NAV_ROUTE_PROFILE, -2);
    y += ScaleUIPx(10);
    DrawUISectionLabel((UISectionLabel){.label = GetLocaleText("settings_title")},
                       area.content_x, y);
    y += ScaleUIPx(28);
    app_sidebar_button(app, area.content_x, &y, area.content_width,
                       GetLocaleText("settings_section_session"), APP_NAV_ROUTE_NONE,
                       SETTINGS_TAB_SESSION);
    app_sidebar_button(app, area.content_x, &y, area.content_width,
                       GetLocaleText("settings_tab_device"), APP_NAV_ROUTE_NONE,
                       SETTINGS_TAB_DEVICE);
    app_sidebar_button(app, area.content_x, &y, area.content_width,
                       GetLocaleText("settings_section_appearance"), APP_NAV_ROUTE_NONE,
                       SETTINGS_TAB_THEME);
    app_sidebar_button(app, area.content_x, &y, area.content_width,
                       GetLocaleText("settings_tab_about"), APP_NAV_ROUTE_NONE,
                       SETTINGS_TAB_ABOUT);
    y += ScaleUIPx(10);
    if(DrawUIGenericButton(area.content_x, y, area.content_width, ScaleUIPx(40),
                              GetLocaleText("customize_nav_button"),
                              UI_BUTTON_STYLE_PRIMARY, app->modal.active, &hover))
        app_open_bottom_nav_config(app);

    EndUIScrollContainer(area, view);
}

static int
app_nav_option_index(int route)
{
    switch(route) {
    case APP_NAV_ROUTE_PROFILE: return 0;
    case APP_NAV_ROUTE_HABITS: return 1;
    case APP_NAV_ROUTE_PRACTICE: return 2;
    case APP_NAV_ROUTE_PET: return 3;
    case APP_NAV_ROUTE_SETTINGS: return 4;
    default: break;
    }
    return 0;
}

static int
app_nav_route_for_option(int option)
{
    static const int routes[] = {
        APP_NAV_ROUTE_PROFILE,
        APP_NAV_ROUTE_HABITS,
        APP_NAV_ROUTE_PRACTICE,
        APP_NAV_ROUTE_PET,
        APP_NAV_ROUTE_SETTINGS
    };

    if(option < 0 || option >= 5)
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

int
app_draw_customize_nav_page(InbeApp *app)
{
    const char *options[5];
    UIScrollPage page;
    int selected[APP_BOTTOM_NAV_CONTENT_MAX];
    int y;
    int changed = 0;
    int button_h = ScaleUIPx(36);
    int gap = ScaleUIPx(8);

    if(app == NULL)
        return 0;
    if(app->bottom_nav_config_route_count < 1 ||
       app->bottom_nav_config_route_count > APP_BOTTOM_NAV_CONTENT_MAX) {
        app_sanitize_bottom_nav_routes(app);
        for(int i = 0; i < app->bottom_nav_route_count; i++)
            app->bottom_nav_config_routes[i] = app->bottom_nav_routes[i];
        app->bottom_nav_config_route_count = app->bottom_nav_route_count;
    }

    if(DrawUIReturnTitleBar(app->icons[UI_ICON_TYPE_RETURN],
                            GetLocaleText("customize_nav_title"),
                            GetUITabBarHeight())) {
        app_switch_screen(app, app->main_tab == APP_MAIN_TAB_HABITS
                                  ? InbeScreenHabits
                                  : InbeScreenStart);
        return 1;
    }

    options[0] = app_nav_route_label(APP_NAV_ROUTE_PROFILE);
    options[1] = app_nav_route_label(APP_NAV_ROUTE_HABITS);
    options[2] = app_nav_route_label(APP_NAV_ROUTE_PRACTICE);
    options[3] = app_nav_route_label(APP_NAV_ROUTE_PET);
    options[4] = app_nav_route_label(APP_NAV_ROUTE_SETTINGS);

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
        int trash_hover = 0;
        int row_h = ScaleUIPx(84);
        int icon_w = ScaleUIPx(36);
        int move_w = ScaleUIPx(58);
        int dropdown_w = page.content_w - icon_w - ScaleUIPx(8);
        int move_y = y + ScaleUIPx(44);
        int dropdown_y = y + ScaleUIPx(22);

        if(dropdown_w < ScaleUIPx(140))
            dropdown_w = page.content_w;
        DrawUIText(GetLocaleText("customize_nav_slot"), page.content_x, y,
                   GetUISmallFontSize(), DarkenUIColor(GetThemeText(), 34));
        DrawUIDropdownButton(740 + i, page.content_x, dropdown_y,
                             dropdown_w, button_h, options, 5, &selected[i]);
        if(dropdown_w < page.content_w) {
            if(DrawUIPaddedIconBtn(page.content_x + page.content_w - icon_w,
                                   dropdown_y - ScaleUIPx(2), ScaleUIPx(20),
                                   ScaleUIPx(8), app->icons[UI_ICON_TYPE_TRASH],
                                   &trash_hover)) {
                for(int j = i; j < app->bottom_nav_config_route_count - 1; j++)
                    app->bottom_nav_config_routes[j] =
                        app->bottom_nav_config_routes[j + 1];
                app->bottom_nav_config_route_count--;
                changed = 1;
                break;
            }
        }
        if(i > 0) {
            int up_hover = 0;
            if(DrawUIGenericButton(page.content_x, move_y, move_w, button_h,
                                      GetLocaleText("move_up_button"),
                                      UI_BUTTON_STYLE_SECONDARY, 0, &up_hover)) {
                int tmp = app->bottom_nav_config_routes[i - 1];
                app->bottom_nav_config_routes[i - 1] =
                    app->bottom_nav_config_routes[i];
                app->bottom_nav_config_routes[i] = tmp;
                changed = 1;
                break;
            }
        }
        if(i + 1 < app->bottom_nav_config_route_count) {
            int down_hover = 0;
            int down_x = page.content_x + (i > 0 ? move_w + gap : 0);
            if(DrawUIGenericButton(down_x, move_y, move_w, button_h,
                                      GetLocaleText("move_down_button"),
                                      UI_BUTTON_STYLE_SECONDARY, 0, &down_hover)) {
                int tmp = app->bottom_nav_config_routes[i + 1];
                app->bottom_nav_config_routes[i + 1] =
                    app->bottom_nav_config_routes[i];
                app->bottom_nav_config_routes[i] = tmp;
                changed = 1;
                break;
            }
        }
        y += row_h;
    }

    if(app->bottom_nav_config_route_count < APP_BOTTOM_NAV_CONTENT_MAX) {
        int add_hover = 0;
        if(DrawUIGenericButton(page.content_x, y, page.content_w, button_h,
                                  GetLocaleText("customize_nav_add"),
                                  UI_BUTTON_STYLE_SECONDARY, 0, &add_hover)) {
            app->bottom_nav_config_routes[app->bottom_nav_config_route_count++] =
                APP_NAV_ROUTE_HABITS;
            changed = 1;
        }
        y += button_h + ScaleUIPx(14);
    }

    {
        int save_hover = 0;
        if(DrawUIGenericButton(page.content_x, y, page.content_w, button_h,
                                  GetLocaleText("save_button"),
                                  UI_BUTTON_STYLE_PRIMARY, 0, &save_hover)) {
            app_save_bottom_nav_config(app);
            app_switch_screen(app, app->main_tab == APP_MAIN_TAB_HABITS
                                      ? InbeScreenHabits
                                      : InbeScreenStart);
        }
    }

    SetUIDropdownClipTop(GetUITabBarHeight() + ScaleUIPx(8));
    SetUIDropdownClipBottom(view_height);
    for(int i = 0; i < app->bottom_nav_config_route_count; i++) {
        if(DrawUIDropdownMenu(740 + i)) {
            app->bottom_nav_config_routes[i] =
                app_nav_route_for_option(selected[i]);
            changed = 1;
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
