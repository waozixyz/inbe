#include "profile_screen.h"

#include "app.h"
#include "text_utils.h"
#include "locale.h"
#include "theme.h"
#include "ui.h"
#include "profile_social.h"
#include "practice_screen.h"
#include "settings/settings_data.h"
#include "settings/settings_sync_account.h"
#include "storage.h"
#include "sync_account.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

extern int view_height;
extern int view_width;

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
    return settings_sync_account_config_content_height(content_w) +
           ScaleUIPx(34) +
           profile_data_content_height(content_w);
}

static int
profile_habits_content_height(InbeApp *app, int content_w)
{
    int row_h = content_w < ScaleUIPx(260) ? ScaleUIPx(78) : ScaleUIPx(48);
    int count = app != NULL ? app->habits.count : 0;

    return ScaleUIPx(16) + (count > 0 ? count * row_h : ScaleUIPx(24));
}

static int
profile_content_height(int content_w, void *user_data)
{
    InbeApp *app = user_data;

    if(app != NULL && app->profile_view == PROFILE_VIEW_DATA)
        return ScaleUIPx(12) + profile_data_content_height(content_w);
    if(app != NULL && app->profile_view == PROFILE_VIEW_SYNC_ACCOUNT)
        return ScaleUIPx(12) + profile_sync_content_height(content_w);
    if(app != NULL && app->profile_view == PROFILE_VIEW_HABITS)
        return ScaleUIPx(12) + profile_habits_content_height(app, content_w);
    if(app != NULL && app->profile_tab == PROFILE_TAB_FRIENDS)
        return ScaleUIPx(12) + profile_social_friends_content_height(app, content_w) +
               ScaleUIPx(54) +
               profile_social_leaderboard_content_height(app, content_w);
    if(app != NULL && app->profile_tab == PROFILE_TAB_LEADERBOARD)
        return ScaleUIPx(12) +
               profile_social_leaderboard_content_height(app, content_w);
    if(app != NULL && app->profile_tab == PROFILE_TAB_DATA)
        return ScaleUIPx(12) + profile_data_content_height(content_w);
    return ScaleUIPx(12) + profile_sync_content_height(content_w);
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
profile_draw_fitted_text(const char *text, int x, int y, int max_w, int font, Color color)
{
    char fitted[INBE_HABIT_NAME_SIZE + 4];

    if(text == NULL || max_w <= 0)
        return;
    inbe_text_fit_ellipsis(text, fitted, sizeof(fitted), max_w, font);
    DrawUIText(fitted, x, y, font, color);
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

int
profile_screen_draw(InbeApp *app)
{
    int header_h = GetUITitleBarHeight();
    int content_y = header_h;
    int content_h = view_height - content_y - app_content_bottom_reserved(app);
    UIScrollPage page;
    int y;
    const char *title = GetLocaleText("sync_configure_account_button");

#if ANDROID_BUILD
    settings_data_handle_android_import(app);
#elif defined(PLATFORM_WEB)
    settings_data_handle_web_import(app);
#endif

    if(app->profile_tab < 0 || app->profile_tab >= PROFILE_TAB_COUNT)
        app->profile_tab = PROFILE_TAB_OVERVIEW;
    if(app->profile_view == PROFILE_VIEW_MAIN &&
       app->profile_tab == PROFILE_TAB_OVERVIEW) {
        app->profile_view = PROFILE_VIEW_SYNC_ACCOUNT;
        app->profile_scroll = 0;
    }
    if(app->profile_view == PROFILE_VIEW_DATA)
        title = GetLocaleText("profile_data_title");
    else if(app->profile_view == PROFILE_VIEW_HABITS)
        title = GetLocaleText("profile_my_habits_title");
    else if(app->profile_tab != PROFILE_TAB_OVERVIEW)
        title = profile_tab_title(app->profile_tab);
    DrawUITitleBar(title, header_h);

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
    y = page.content_y + ScaleUIPx(12);

    if(app->profile_view == PROFILE_VIEW_SYNC_ACCOUNT) {
        settings_sync_account_draw_config(app, page.content_x, page.content_w, &y);
        y += ScaleUIPx(18);
        profile_draw_divider(page.content_x, page.content_w, y);
        y += ScaleUIPx(16);
        DrawUIText(GetLocaleText("profile_data_title"), page.content_x, y,
                   GetUIFontSize(), GetThemeText());
        y += ScaleUIPx(28);
        profile_draw_data(app, page.content_x, page.content_w, &y);
    }
    else if(app->profile_view == PROFILE_VIEW_DATA)
        profile_draw_data(app, page.content_x, page.content_w, &y);
    else if(app->profile_view == PROFILE_VIEW_HABITS)
        profile_draw_habits(app, page.content_x, page.content_w, &y);
    else if(app->profile_tab == PROFILE_TAB_FRIENDS) {
        profile_social_draw_friends(app, page.content_x, page.content_w, &y);
        y += ScaleUIPx(22);
        profile_draw_divider(page.content_x, page.content_w, y);
        y += ScaleUIPx(16);
        DrawUIText(GetLocaleText("profile_leaderboard_title"), page.content_x, y,
                   GetUIFontSize(), GetThemeText());
        y += ScaleUIPx(18);
        profile_social_draw_leaderboard(app, page.content_x, page.content_w, &y);
    }
    else if(app->profile_tab == PROFILE_TAB_LEADERBOARD)
        profile_social_draw_leaderboard(app, page.content_x, page.content_w, &y);
    else if(app->profile_tab == PROFILE_TAB_DATA)
        profile_draw_data(app, page.content_x, page.content_w, &y);
    else
        settings_sync_account_draw_config(app, page.content_x, page.content_w, &y);

    EndUIScrollPage(page);
    if(app->profile_view == PROFILE_VIEW_MAIN &&
       (app->profile_tab == PROFILE_TAB_LEADERBOARD ||
        app->profile_tab == PROFILE_TAB_FRIENDS)) {
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
