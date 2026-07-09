#include "profile_screen.h"

#include "app.h"
#include "text_utils.h"
#include "locale.h"
#include "theme.h"
#include "ui.h"
#include "profile_social.h"
#include "practice_screen.h"
#include "settings/settings_data.h"
#include "settings/settings_screen.h"
#include "settings/settings_sync_account.h"
#include "storage.h"
#include "sync_account.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

extern int view_height;
extern int view_width;

#define INBE_SYNC_SERVER_URL_KEY "sync_server_url"

enum {
    PROFILE_GUIDE_STEPS = 3
};

static Rectangle
profile_expanded_anchor(int x, int y, int w, int h)
{
    int pad = ScaleUIPx(6);

    return (Rectangle){(float)(x - pad), (float)(y - pad),
                       (float)(w + pad * 2), (float)(h + pad * 2)};
}

static void
profile_set_guide_anchor(Rectangle *anchor, int x, int y, int w, int h)
{
    if(anchor == NULL)
        return;
    *anchor = profile_expanded_anchor(x, y, w, h);
}

static void
profile_format_size(char *out, size_t out_size, long long bytes)
{
    if(bytes >= 1024LL * 1024LL)
        snprintf(out, out_size, "%lld MB",
                 (bytes + 1024LL * 1024LL / 2) / (1024LL * 1024LL));
    else if(bytes >= 1024)
        snprintf(out, out_size, "%lld KB", (bytes + 1024 / 2) / 1024);
    else
        snprintf(out, out_size, "%lld B", bytes);
}

static int
profile_main_content_height(int content_w)
{
    int h = ScaleUIPx(14);
    int btn_h = ScaleUIPx(34);

    if(content_w < ScaleUIPx(320))
        h += ScaleUIPx(18) + ScaleUIPx(30) + btn_h + ScaleUIPx(10);
    else
        h += ScaleUIPx(18) + ScaleUIPx(38);
    h += ScaleUIPx(18);
    h += ScaleUIPx(24) + ScaleUIPx(54) + btn_h + ScaleUIPx(8) +
         btn_h + ScaleUIPx(20);
    h += ScaleUIPx(18);
    h += ScaleUIPx(20) + ScaleUIPx(34) + btn_h + ScaleUIPx(20);
    return h;
}

static int
profile_data_content_height(int content_w)
{
    int height = settings_data_content_height(content_w) + ScaleUIPx(64);

    if(sync_account_load(&(InbeSyncAccount){0}))
        height += ScaleUIPx(86);
    return height;
}

static int
profile_sync_content_height(int content_w)
{
    return settings_sync_account_config_content_height(content_w);
}

static int
profile_habits_content_height(InbeApp *app, int content_w)
{
    int row_h = content_w < ScaleUIPx(260) ? ScaleUIPx(78) : ScaleUIPx(48);
    int count = app != NULL ? app->habits.count : 0;

    return ScaleUIPx(16) + (count > 0 ? count * row_h : ScaleUIPx(24));
}

static int
profile_practices_content_height(int content_w)
{
    (void)content_w;
    return ScaleUIPx(16) +
           GetUICheckboxRowHeight((UICheckboxRow){0}) * EXERCISE_COUNT +
           ScaleUIPx(16);
}

static int
profile_content_height(int content_w, void *user_data)
{
    InbeApp *app = user_data;

    if(app != NULL && app->profile_view == PROFILE_VIEW_DATA)
        return profile_data_content_height(content_w);
    if(app != NULL && app->profile_view == PROFILE_VIEW_SYNC_ACCOUNT)
        return profile_sync_content_height(content_w);
    if(app != NULL && app->profile_view == PROFILE_VIEW_HABITS)
        return profile_habits_content_height(app, content_w);
    if(app != NULL && app->profile_view == PROFILE_VIEW_PRACTICES)
        return profile_practices_content_height(content_w);
    if(app != NULL && app->profile_tab == PROFILE_TAB_FRIENDS)
        return profile_social_friends_content_height(app, content_w);
    if(app != NULL && app->profile_tab == PROFILE_TAB_LEADERBOARD)
        return profile_social_leaderboard_content_height(app, content_w);
    if(app != NULL && app->profile_tab == PROFILE_TAB_DATA)
        return profile_data_content_height(content_w);
    return profile_main_content_height(content_w);
}

static const char *
profile_practice_label(int index)
{
    switch(index) {
        case EXERCISE_MEDITATION:
            return GetLocaleText("exercise_meditation");
        case EXERCISE_SUN_SALUTATION:
            return GetLocaleText("exercise_sun_salutation");
        case EXERCISE_WIM_HOF:
        default:
            return GetLocaleText("exercise_wim_hof");
    }
}

static void
profile_draw_stat_row(int x, int w, int *y, const char *label, const char *value)
{
    int font = GetUIFontSize();
    int small_font = GetUISmallFontSize();
    int value_w;

    DrawUIText(label, x, *y, small_font, DarkenUIColor(GetThemeText(), 35));
    value_w = MeasureUIText(value, font);
    DrawUIText(value, x + w - value_w, *y - ScaleUIPx(2), font, GetThemeText());
    *y += ScaleUIPx(28);
}

static void
profile_draw_data_summary(int x, int w, int *y)
{
    char value[64];

    snprintf(value, sizeof(value), "%d", storage_habit_count());
    profile_draw_stat_row(x, w, y, GetLocaleText("profile_habits_label"), value);
    snprintf(value, sizeof(value), "%d", storage_session_count());
    profile_draw_stat_row(x, w, y, GetLocaleText("profile_sessions_label"), value);
    profile_format_size(value, sizeof(value), storage_total_size());
    profile_draw_stat_row(x, w, y, GetLocaleText("profile_storage_label"), value);
}

static void
profile_draw_divider(int x, int w, int y)
{
    DrawLine(x, y, x + w, y, DarkenUIColor(GetThemeBackground(), 28));
}

static const char *
profile_tab_title(int tab)
{
    switch(tab) {
        case PROFILE_TAB_FRIENDS:
            return GetLocaleText("profile_friends_title");
        case PROFILE_TAB_LEADERBOARD:
            return GetLocaleText("profile_leaderboard_title");
        case PROFILE_TAB_DATA:
            return GetLocaleText("profile_data_title");
        case PROFILE_TAB_OVERVIEW:
        default:
            return GetLocaleText("tab_profile");
    }
}

static void
profile_screen_finish_first_run_guide(InbeApp *app)
{
    if(app == NULL)
        return;
    app->profile_guide_seen = 1;
    app->profile_guide_step = 0;
    save_settings(app);
}

int
profile_screen_first_run_guide_active(const InbeApp *app)
{
    InbeSyncAccount account;

    return app != NULL && !app->profile_guide_seen && !app->modal.active &&
           app->inbe.screen == InbeScreenProfile &&
           !sync_account_load(&account);
}

void
profile_screen_prepare_first_run_guide(InbeApp *app)
{
    if(!profile_screen_first_run_guide_active(app))
        return;
    app->profile_guide_step = clampi(app->profile_guide_step, 0, PROFILE_GUIDE_STEPS - 1);
    app->profile_view = PROFILE_VIEW_MAIN;
    app->profile_tab = PROFILE_TAB_OVERVIEW;
    app->profile_scroll = 0;
}

void
profile_screen_draw_first_run_guide(InbeApp *app)
{
    UIGuideStep steps[PROFILE_GUIDE_STEPS];
    UIGuideResult result;

    if(!profile_screen_first_run_guide_active(app))
        return;
    if(!app->profile_guide_anchors.valid)
        return;

    steps[0] = (UIGuideStep){
        .anchor = app->profile_guide_anchors.account,
        .text = GetLocaleText("profile_guide_account")
    };
    steps[1] = (UIGuideStep){
        .anchor = app->profile_guide_anchors.data,
        .text = GetLocaleText("profile_guide_data")
    };
    steps[2] = (UIGuideStep){
        .anchor = app->profile_guide_anchors.social,
        .text = GetLocaleText("profile_guide_social_no_account")
    };

    result = DrawUIGuideOverlay((UIGuideOverlay){
        .steps = steps,
        .count = PROFILE_GUIDE_STEPS,
        .step = &app->profile_guide_step,
        .view_width = view_width,
        .view_height = view_height,
        .reserved_top = 0,
        .reserved_bottom = GetUIBottomNavHeight(),
        .max_width = ScaleUIPx(300),
        .paragraph_font = GetUISmallFontSize(),
        .close_icon = app->icons[UI_ICON_TYPE_X],
        .back_icon = app->icons[UI_ICON_TYPE_BACKWARD],
        .next_icon = app->icons[UI_ICON_TYPE_FORWARD],
        .done_icon = app->icons[UI_ICON_TYPE_CHECK]
    });
    if(result.closed || result.finished)
        profile_screen_finish_first_run_guide(app);
}

static void
profile_draw_summary_columns(int x, int w, int *y)
{
    char habits[32];
    char sessions[32];
    char storage[64];
    const char *labels[3] = {
        GetLocaleText("profile_habits_label"),
        GetLocaleText("profile_sessions_label"),
        GetLocaleText("profile_storage_label")
    };
    const char *values[3] = {habits, sessions, storage};
    int col_w = w / 3;
    int value_font = UI_TEXT_16;
    int label_font = GetUISmallFontSize();

    snprintf(habits, sizeof(habits), "%d", storage_habit_count());
    snprintf(sessions, sizeof(sessions), "%d", storage_session_count());
    profile_format_size(storage, sizeof(storage), storage_total_size());

    for(int i = 0; i < 3; i++) {
        int cx = x + i * col_w;
        int cw = i == 2 ? x + w - cx : col_w;
        DrawUIText(values[i], cx, *y, value_font, GetThemeText());
        DrawUIText(labels[i], cx, *y + ScaleUIPx(24), label_font,
                        DarkenUIColor(GetThemeText(), 32));
        if(i < 2)
            DrawLine(cx + cw - ScaleUIPx(10), *y - ScaleUIPx(2),
                     cx + cw - ScaleUIPx(10), *y + ScaleUIPx(42),
                     DarkenUIColor(GetThemeBackground(), 24));
    }
    *y += ScaleUIPx(58);
}

static void
profile_draw_fitted_text(const char *text, int x, int y, int max_w, int font, Color color)
{
    char fitted[INBE_HABIT_NAME_SIZE + 4];

    if(text == NULL || max_w <= 0)
        return;
    inbe_text_fit_ellipsis(text, fitted, sizeof(fitted), max_w, font);
    DrawUIText(fitted, x, y, font, color);
}

static void
profile_switch_view(InbeApp *app, int view)
{
    AppRoute route;

    if(app == NULL)
        return;
    route = app_current_route(app);
    route.profile_view = view;
    if(view != PROFILE_VIEW_MAIN)
        route.profile_tab = PROFILE_TAB_OVERVIEW;
    app->profile_scroll = 0;
    app->sync_server_url_focused = 0;
    settings_screen_clear_status();
    app_switch_route(app, route);
}

static void
profile_switch_tab(InbeApp *app, int tab)
{
    AppRoute route;

    if(app == NULL)
        return;
    route = app_current_route(app);
    route.profile_view = PROFILE_VIEW_MAIN;
    route.profile_tab = tab;
    app->profile_scroll = 0;
    settings_screen_clear_status();
    app_switch_route(app, route);
}

static void
profile_draw_overview(InbeApp *app, int x, int w, int *y)
{
    int font = GetUIFontSize();
    int small = GetUISmallFontSize();
    int btn_h = ScaleUIPx(34);
    int account_y;
    int data_y;
    int social_y;
    int hover_account = 0;
    int hover_data = 0;
    int hover_practices = 0;
    int hover_friends = 0;
    int hover_leaderboard = 0;
    int half_w = (w - ScaleUIPx(8)) / 2;
    InbeSyncAccount account;
    int has_account;
    const char *alias;
    char account_text[96];
    char pending_text[64];
    char friend_text[64];

    *y += ScaleUIPx(14);
    account_y = *y;
    has_account = sync_account_load(&account);
    alias = storage_get_setting_text("sync_account_alias");
    account_text[0] = '\0';
    if(has_account) {
        if(alias != NULL && alias[0] != '\0') {
            snprintf(account_text, sizeof(account_text), "@%s", alias);
        } else {
            snprintf(account_text, sizeof(account_text), "%.12s...", account.public_id);
        }
    } else {
        snprintf(account_text, sizeof(account_text), "%s", GetLocaleText("profile_no_account"));
    }

    DrawUIText(GetLocaleText("profile_account_section"), x, *y, small,
                    DarkenUIColor(GetThemeText(), 35));
    *y += ScaleUIPx(18);
    if(w < ScaleUIPx(320)) {
        profile_draw_fitted_text(account_text, x, *y, w, font, GetThemeText());
        *y += ScaleUIPx(30);
        if(DrawUIGenericButton(x, *y, w, btn_h, GetLocaleText("profile_configure_button"),
                                  UI_BUTTON_STYLE_SECONDARY, 0,
                                  &hover_account))
            settings_data_open_sync_account_config(app);
        *y += btn_h + ScaleUIPx(10);
    } else {
        int button_w = ScaleUIPx(132);
        int text_w = w - button_w - ScaleUIPx(10);
        profile_draw_fitted_text(account_text, x, *y, text_w, font,
                                 GetThemeText());
        if(DrawUIGenericButton(x + w - button_w, *y - ScaleUIPx(8), button_w,
                                  btn_h, GetLocaleText("profile_configure_button"),
                                  UI_BUTTON_STYLE_SECONDARY, 0,
                                  &hover_account))
            settings_data_open_sync_account_config(app);
        *y += ScaleUIPx(38);
    }
    profile_draw_divider(x, w, *y);
    profile_set_guide_anchor(&app->profile_guide_anchors.account, x, account_y, w,
                             *y - account_y);
    *y += ScaleUIPx(18);

    data_y = *y;
    DrawUIText(GetLocaleText("profile_data_section"), x, *y, small,
                    DarkenUIColor(GetThemeText(), 35));
    *y += ScaleUIPx(24);
    profile_draw_summary_columns(x, w, y);
    if(DrawUIGenericButton(x, *y, w, btn_h, GetLocaleText("profile_data_button"),
                              UI_BUTTON_STYLE_SECONDARY, 0, &hover_data))
        profile_switch_tab(app, PROFILE_TAB_DATA);
    *y += btn_h + ScaleUIPx(8);
    if(DrawUIGenericButton(x, *y, w, btn_h,
                              GetLocaleText("profile_my_practices_button"),
                              UI_BUTTON_STYLE_SECONDARY, 0, &hover_practices))
        profile_switch_view(app, PROFILE_VIEW_PRACTICES);
    *y += btn_h + ScaleUIPx(20);
    profile_draw_divider(x, w, *y);
    profile_set_guide_anchor(&app->profile_guide_anchors.data, x, data_y, w,
                             *y - data_y);
    *y += ScaleUIPx(18);

    social_y = *y;
    snprintf(friend_text, sizeof(friend_text), GetLocaleText("profile_friends_count_format"),
             profile_social_friends_count(app));
    snprintf(pending_text, sizeof(pending_text), GetLocaleText("profile_pending_count_format"),
             profile_social_pending_count(app));
    DrawUIText(GetLocaleText("profile_social_section"), x, *y, small,
                    DarkenUIColor(GetThemeText(), 35));
    *y += ScaleUIPx(20);
    DrawUIText(friend_text, x, *y, font, GetThemeText());
    DrawUIText(pending_text, x + w - MeasureUIText(pending_text, font), *y,
                    font, GetThemeText());
    *y += ScaleUIPx(34);
    if(DrawUIGenericButton(x, *y, half_w, btn_h, GetLocaleText("profile_friends_title"),
                              UI_BUTTON_STYLE_PRIMARY, !has_account, &hover_friends))
        profile_switch_tab(app, PROFILE_TAB_FRIENDS);
    if(DrawUIGenericButton(x + half_w + ScaleUIPx(8), *y, half_w, btn_h,
                              GetLocaleText("profile_leaderboard_title"), UI_BUTTON_STYLE_PRIMARY,
                              !has_account, &hover_leaderboard))
        profile_switch_tab(app, PROFILE_TAB_LEADERBOARD);
    *y += btn_h + ScaleUIPx(16);
    profile_set_guide_anchor(&app->profile_guide_anchors.social, x, social_y, w,
                             *y - social_y);
    app->profile_guide_anchors.valid = 1;
}

static void
profile_draw_data(InbeApp *app, int x, int w, int *y)
{
    profile_draw_data_summary(x, w, y);
    settings_data_draw_sync_status(x, w, y);
    *y += ScaleUIPx(10);
    settings_data_draw_actions(app, x, w, y);
}

static void
profile_draw_habits(InbeApp *app, int x, int w, int *y)
{
    int font = GetUIFontSize();
    int small = GetUISmallFontSize();
    int btn_h = ScaleUIPx(32);
    int btn_w = ScaleUIPx(74);
    int gap = ScaleUIPx(8);
    int stack_rows = w < ScaleUIPx(260);

    *y += ScaleUIPx(16);

    for(int i = 0; i < app->habits.count; i++) {
        int row_y = *y;
        int up_hover = 0;
        int down_hover = 0;
        int controls_w = btn_w * 2 + gap;
        int name_w = stack_rows ? w : w - controls_w - gap;

        profile_draw_fitted_text(app->habits.items[i].name, x, row_y + ScaleUIPx(7),
                                 name_w, font, GetThemeText());
        if(stack_rows)
            row_y += ScaleUIPx(36);
        if(DrawUIGenericButton(stack_rows ? x : x + w - btn_w * 2 - gap,
                                  row_y, btn_w, btn_h,
                                  GetLocaleText("move_up_button"),
                                  UI_BUTTON_STYLE_SECONDARY, i == 0, &up_hover)) {
            if(habits_move(&app->habits, i, i - 1))
                app_auto_sync(app);
            return;
        }
        if(DrawUIGenericButton(stack_rows ? x + btn_w + gap : x + w - btn_w,
                                  row_y, btn_w, btn_h,
                                  GetLocaleText("move_down_button"),
                                  UI_BUTTON_STYLE_SECONDARY,
                                  i == app->habits.count - 1, &down_hover)) {
            if(habits_move(&app->habits, i, i + 1))
                app_auto_sync(app);
            return;
        }
        *y += stack_rows ? ScaleUIPx(78) : ScaleUIPx(46);
        profile_draw_divider(x, w, *y - ScaleUIPx(6));
    }

    if(app->habits.count <= 0)
        DrawUIText(GetLocaleText("habit_empty_title"), x, *y, small,
                        DarkenUIColor(GetThemeText(), 35));
}

static void
profile_draw_practices(InbeApp *app, int x, int w, int *y)
{
    (void)w;
    *y += ScaleUIPx(16);

    for(int i = 0; i < EXERCISE_COUNT; i++) {
        int enabled = practice_is_visible(app, i);
        if(DrawUICheckboxRow((UICheckboxRow){
            .label = profile_practice_label(i),
            .value = &enabled
        }, x, *y)) {
            practice_set_visible(app, i, enabled);
        }
        *y += GetUICheckboxRowHeight((UICheckboxRow){0});
    }
}

int
profile_screen_draw(InbeApp *app)
{
    int is_profile_subpage = app->profile_view == PROFILE_VIEW_MAIN &&
                             app->profile_tab != PROFILE_TAB_OVERVIEW;
    int header_h = (app->profile_view != PROFILE_VIEW_MAIN || is_profile_subpage)
                       ? GetUITitleBarHeight()
                       : 0;
    int content_y = header_h;
    int content_h = view_height - content_y - app_content_bottom_reserved(app);
    UIScrollPage page;
    int y;

    app->profile_guide_anchors.valid = 0;

#if ANDROID_BUILD
    settings_data_handle_android_import(app);
#elif defined(PLATFORM_WEB)
    settings_data_handle_web_import(app);
#endif

    if(app->profile_tab < 0 || app->profile_tab >= PROFILE_TAB_COUNT)
        app->profile_tab = PROFILE_TAB_OVERVIEW;
    if((app->profile_tab == PROFILE_TAB_FRIENDS ||
        app->profile_tab == PROFILE_TAB_LEADERBOARD) &&
       !sync_account_load(&(InbeSyncAccount){0})) {
        app->profile_tab = PROFILE_TAB_OVERVIEW;
        app->profile_scroll = 0;
    }

    if(app->profile_view != PROFILE_VIEW_MAIN) {
        const char *title = GetLocaleText("profile_data_title");
        if(app->profile_view == PROFILE_VIEW_SYNC_ACCOUNT)
            title = GetLocaleText("sync_configure_account_button");
        else if(app->profile_view == PROFILE_VIEW_HABITS)
            title = GetLocaleText("profile_my_habits_title");
        else if(app->profile_view == PROFILE_VIEW_PRACTICES)
            title = GetLocaleText("profile_my_practices_title");
        if(DrawUIReturnTitleBar(app->icons[UI_ICON_TYPE_RETURN], title, header_h))
            profile_switch_view(app, PROFILE_VIEW_MAIN);
    } else if(is_profile_subpage) {
        if(DrawUIReturnTitleBar(app->icons[UI_ICON_TYPE_RETURN], profile_tab_title(app->profile_tab), header_h))
            profile_switch_tab(app, PROFILE_TAB_OVERVIEW);
    }

    if(content_h < 0)
        content_h = 0;

    page = BeginUIScrollPage((UIScrollPageSpec){
        .y = content_y,
        .height = content_h,
        .max_content_width = ScaleUIPx(CONTENT_MAX_W),
        .min_content_width = ScaleUIPx(320),
        .scroll_offset = &app->profile_scroll,
        .content_height = profile_content_height,
        .user_data = app
    });
    y = page.content_y;

    if(app->profile_view == PROFILE_VIEW_SYNC_ACCOUNT)
        settings_sync_account_draw_config(app, page.content_x, page.content_w, &y);
    else if(app->profile_view == PROFILE_VIEW_DATA)
        profile_draw_data(app, page.content_x, page.content_w, &y);
    else if(app->profile_view == PROFILE_VIEW_HABITS)
        profile_draw_habits(app, page.content_x, page.content_w, &y);
    else if(app->profile_view == PROFILE_VIEW_PRACTICES)
        profile_draw_practices(app, page.content_x, page.content_w, &y);
    else if(app->profile_tab == PROFILE_TAB_FRIENDS)
        profile_social_draw_friends(app, page.content_x, page.content_w, &y);
    else if(app->profile_tab == PROFILE_TAB_LEADERBOARD)
        profile_social_draw_leaderboard(app, page.content_x, page.content_w, &y);
    else if(app->profile_tab == PROFILE_TAB_DATA)
        profile_draw_data(app, page.content_x, page.content_w, &y);
    else
        profile_draw_overview(app, page.content_x, page.content_w, &y);

    EndUIScrollPage(page);
    if(app->profile_view == PROFILE_VIEW_MAIN &&
       app->profile_tab == PROFILE_TAB_LEADERBOARD) {
        SetUIDropdownClipTop(content_y);
        SetUIDropdownClipBottom(view_height - app_content_bottom_reserved(app));
        if(DrawUIDropdownMenu(811)) {
            profile_social_load_leaderboard_cache(app);
            app->profile_scroll = 0;
        }
        SetUIDropdownClipTop(0);
        SetUIDropdownClipBottom(0);
    }
    return 0;
}
