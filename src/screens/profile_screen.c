#include "profile_screen.h"

#include "app.h"
#include "flint_locale.h"
#include "flint_theme.h"
#include "flint_ui.h"
#include "practice_screen.h"
#include "settings/settings_data.h"
#include "settings/settings_screen.h"
#include "settings/settings_sync_account.h"
#include "storage.h"
#include "sync_account.h"
#include "sync_client.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

extern int view_height;
extern int view_width;

#define INBE_SYNC_SERVER_URL_KEY "sync_server_url"

enum {
    PROFILE_GUIDE_STEPS = 3,
    PROFILE_LEADERBOARD_STREAK = 0,
    PROFILE_LEADERBOARD_AVG_HOLD,
    PROFILE_LEADERBOARD_METRIC_COUNT
};

static void
profile_format_size(char *out, size_t out_size, long long bytes)
{
    if(bytes >= 1024LL * 1024LL)
        snprintf(out, out_size, "%.1f MB", (double)bytes / (1024.0 * 1024.0));
    else if(bytes >= 1024)
        snprintf(out, out_size, "%.1f KB", (double)bytes / 1024.0);
    else
        snprintf(out, out_size, "%lld B", bytes);
}

static int
profile_main_content_height(int content_w)
{
    (void)content_w;
    return flint_px(700);
}

static int
profile_data_content_height(int content_w)
{
    int height = settings_data_content_height(content_w) + flint_px(104);

    if(sync_account_load(&(InbeSyncAccount){0}))
        height += flint_px(86);
    return height;
}

static int
profile_sync_content_height(int content_w)
{
    (void)content_w;
    return flint_px(430);
}

static int
profile_habits_content_height(int content_w)
{
    (void)content_w;
    return flint_px(90) + flint_px(48) * INBE_HABIT_MAX;
}

static int
profile_practices_content_height(int content_w)
{
    (void)content_w;
    return flint_px(92) + flint_px(46) * EXERCISE_COUNT;
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
        return profile_habits_content_height(content_w);
    if(app != NULL && app->profile_view == PROFILE_VIEW_PRACTICES)
        return profile_practices_content_height(content_w);
    if(app != NULL && app->profile_tab == PROFILE_TAB_FRIENDS)
        return flint_px(740);
    if(app != NULL && app->profile_tab == PROFILE_TAB_LEADERBOARD)
        return flint_px(660);
    if(app != NULL && app->profile_tab == PROFILE_TAB_DATA)
        return profile_data_content_height(content_w);
    return profile_main_content_height(content_w);
}

static FlintUITextInputStyle
profile_text_style(void)
{
    return (FlintUITextInputStyle){
        .background = flint_darken(flint_theme_get_bg(), 6),
        .border = flint_darken(flint_theme_get_bg(), 34),
        .focus_border = flint_theme_get_button(),
        .text = flint_theme_get_text(),
        .cursor = flint_theme_get_text(),
        .radius = 0.0f,
        .padding_x = flint_px(10)
    };
}

static int
profile_friend_filter(int codepoint, void *user_data)
{
    (void)user_data;
    return codepoint > 32 && codepoint < 127;
}

static int
profile_sync_url(char *out, size_t out_size)
{
    const char *saved = storage_get_setting_text(INBE_SYNC_SERVER_URL_KEY);

    if(saved == NULL || saved[0] == '\0')
        return 0;
    return sync_client_normalize_url(saved, out, out_size);
}

static const char *
profile_practice_id(int index)
{
    switch(index) {
        case EXERCISE_MEDITATION:
            return "meditation";
        case EXERCISE_SUN_SALUTATION:
            return "sun_salutation";
        case EXERCISE_WIM_HOF:
        default:
            return "whm";
    }
}

static const char *
profile_practice_label(int index)
{
    switch(index) {
        case EXERCISE_MEDITATION:
            return locale_get("exercise_meditation");
        case EXERCISE_SUN_SALUTATION:
            return locale_get("exercise_sun_salutation");
        case EXERCISE_WIM_HOF:
        default:
            return locale_get("exercise_wim_hof");
    }
}

static const char *
profile_leaderboard_metric_id_for_practice(int practice, int metric)
{
    if(metric != PROFILE_LEADERBOARD_AVG_HOLD)
        return "streak";
    if(practice == EXERCISE_MEDITATION)
        return "avg_time";
    return "avg_hold";
}

static int
profile_leaderboard_metric_count(int practice)
{
    return practice == EXERCISE_SUN_SALUTATION ? 1 : PROFILE_LEADERBOARD_METRIC_COUNT;
}

static void
profile_format_leaderboard_time(long seconds, char *out, size_t out_size)
{
    if(out == NULL || out_size == 0)
        return;
    if(seconds < 0)
        seconds = 0;
    snprintf(out, out_size, "%ld:%02ld", seconds / 3600, (seconds % 3600) / 60);
}

static const char *
profile_leaderboard_metric_label(int practice, int metric)
{
    if(metric != PROFILE_LEADERBOARD_AVG_HOLD)
        return locale_get("profile_metric_streak");
    return practice == EXERCISE_MEDITATION
               ? locale_get("profile_metric_avg_time")
               : locale_get("profile_metric_avg_hold");
}

static void
profile_leaderboard_cache_key(char *out, size_t out_size, int practice, int metric)
{
    if(out == NULL || out_size == 0)
        return;
    snprintf(out, out_size, "leaderboard.inbe.%s.%s",
             profile_practice_id(practice),
             profile_leaderboard_metric_id_for_practice(practice, metric));
}

static void
profile_load_friends_cache(InbeApp *app)
{
    if(app == NULL)
        return;
    if(!storage_get_social_cache_json("friends.requests",
                                      app->profile_friend_requests_json,
                                      sizeof(app->profile_friend_requests_json)))
        snprintf(app->profile_friend_requests_json,
                 sizeof(app->profile_friend_requests_json),
                 "{\"incoming\":[],\"outgoing\":[]}");
    if(!storage_get_social_cache_json("friends.list", app->profile_friends_json,
                                      sizeof(app->profile_friends_json)))
        snprintf(app->profile_friends_json, sizeof(app->profile_friends_json),
                 "{\"friends\":[]}");
    app->profile_friends_loaded = 1;
}

static void
profile_load_leaderboard_cache(InbeApp *app)
{
    char key[96];

    if(app == NULL)
        return;
    profile_leaderboard_cache_key(key, sizeof(key),
                                  app->profile_leaderboard_practice,
                                  app->profile_leaderboard_metric);
    if(!storage_get_social_cache_json(key, app->profile_leaderboard_json,
                                      sizeof(app->profile_leaderboard_json)))
        snprintf(app->profile_leaderboard_json, sizeof(app->profile_leaderboard_json),
                 "{\"rows\":[]}");
    app->profile_leaderboard_loaded = 1;
}

static const char *
profile_json_string_value(const char *object, const char *key, char *out, size_t out_size)
{
    char pattern[48];
    const char *p;
    char *w;
    size_t left;

    if(out == NULL || out_size == 0)
        return NULL;
    out[0] = '\0';
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    p = strstr(object, pattern);
    if(p == NULL)
        return NULL;
    p = strchr(p + strlen(pattern), ':');
    if(p == NULL)
        return NULL;
    p++;
    while(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    if(*p++ != '"')
        return NULL;
    w = out;
    left = out_size - 1;
    while(*p != '\0' && *p != '"' && left > 0) {
        if(*p == '\\' && p[1] != '\0')
            p++;
        *w++ = *p++;
        left--;
    }
    *w = '\0';
    return *p == '"' ? p + 1 : NULL;
}

static double
profile_json_number_value(const char *object, const char *key)
{
    char pattern[48];
    const char *p;

    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    p = strstr(object, pattern);
    if(p == NULL)
        return 0.0;
    p = strchr(p + strlen(pattern), ':');
    if(p == NULL)
        return 0.0;
    return atof(p + 1);
}

static void profile_refresh_friends(InbeApp *app);

static void
profile_draw_json_people(const char *json, const char *array_key,
                         int x, int w, int *y, int with_actions, InbeApp *app)
{
    char key_pattern[48];
    const char *p;
    const char *end;
    int rows = 0;
    int font = flint_ui_font();
    int small = flint_ui_font_small();

    snprintf(key_pattern, sizeof(key_pattern), "\"%s\"", array_key);
    p = strstr(json != NULL ? json : "", key_pattern);
    if(p == NULL) {
        flint_text_draw(locale_get("profile_none_label"), x, *y, small,
                        flint_darken(flint_theme_get_text(), 35));
        *y += flint_px(24);
        return;
    }
    p = strchr(p, '[');
    end = p != NULL ? strchr(p, ']') : NULL;
    if(p == NULL || end == NULL) {
        flint_text_draw(locale_get("profile_none_label"), x, *y, small,
                        flint_darken(flint_theme_get_text(), 35));
        *y += flint_px(24);
        return;
    }
    while((p = strchr(p, '{')) != NULL && p < end && rows < 8) {
        char id[80];
        char alias[48];
        char display_id[80];
        char title[96];
        int row_h = with_actions ? flint_px(42) : flint_px(30);
        int hover_accept = 0;
        int hover_decline = 0;
        char url[256];

        id[0] = '\0';
        alias[0] = '\0';
        display_id[0] = '\0';
        if(profile_json_string_value(p, "id", id, sizeof(id)) == NULL &&
           profile_json_string_value(p, "user_id_hash", id, sizeof(id)) == NULL)
            break;
        if(strcmp(array_key, "incoming") == 0) {
            profile_json_string_value(p, "requester_alias", alias, sizeof(alias));
            profile_json_string_value(p, "requester_user_id_hash", display_id,
                                      sizeof(display_id));
        } else if(strcmp(array_key, "outgoing") == 0) {
            profile_json_string_value(p, "target_alias", alias, sizeof(alias));
            profile_json_string_value(p, "target_user_id_hash", display_id,
                                      sizeof(display_id));
        } else {
            profile_json_string_value(p, "alias", alias, sizeof(alias));
            profile_json_string_value(p, "user_id_hash", display_id, sizeof(display_id));
        }
        if(alias[0] == '\0')
            profile_json_string_value(p, "alias", alias, sizeof(alias));
        if(display_id[0] == '\0')
            snprintf(display_id, sizeof(display_id), "%s", id);
        snprintf(title, sizeof(title), "%s%s", alias[0] != '\0' ? "@" : "",
                 alias[0] != '\0' ? alias : display_id);
        flint_text_draw(title, x, *y, font, flint_theme_get_text());
        if(with_actions) {
            int btn_w = (w - flint_px(8)) / 2;
            if(ui_draw_generic_button(x, *y + flint_px(22), btn_w, flint_px(30),
                                      locale_get("profile_friend_accept_button"),
                                      UI_BUTTON_STYLE_PRIMARY, 0, &hover_accept) &&
               profile_sync_url(url, sizeof(url))) {
                app_request_friend_accept(app, id);
                settings_screen_set_status_success(locale_get("profile_updating_status"), NULL);
            }
            if(ui_draw_generic_button(x + btn_w + flint_px(8), *y + flint_px(22), btn_w,
                                      flint_px(30),
                                      locale_get("profile_friend_decline_button"),
                                      UI_BUTTON_STYLE_SECONDARY,
                                      0, &hover_decline) &&
               profile_sync_url(url, sizeof(url))) {
                app_request_friend_decline(app, id);
                settings_screen_set_status_success(locale_get("profile_updating_status"), NULL);
            }
        }
        *y += row_h;
        rows++;
        p++;
    }
    if(rows == 0) {
        flint_text_draw(locale_get("profile_none_label"), x, *y, small,
                        flint_darken(flint_theme_get_text(), 35));
        *y += flint_px(24);
    }
}

static void
profile_draw_stat_row(int x, int w, int *y, const char *label, const char *value)
{
    int font = flint_ui_font();
    int small_font = flint_ui_font_small();
    int value_w;

    flint_text_draw(label, x, *y, small_font, flint_darken(flint_theme_get_text(), 35));
    value_w = flint_text_measure(value, font);
    flint_text_draw(value, x + w - value_w, *y - flint_px(2), font, flint_theme_get_text());
    *y += flint_px(28);
}

static void
profile_draw_data_summary(int x, int w, int *y)
{
    char value[64];

    snprintf(value, sizeof(value), "%d", storage_habit_count());
    profile_draw_stat_row(x, w, y, locale_get("profile_habits_label"), value);
    snprintf(value, sizeof(value), "%d", storage_session_count());
    profile_draw_stat_row(x, w, y, locale_get("profile_sessions_label"), value);
    profile_format_size(value, sizeof(value), storage_total_size());
    profile_draw_stat_row(x, w, y, locale_get("profile_storage_label"), value);
}

static void
profile_draw_divider(int x, int w, int y)
{
    DrawLine(x, y, x + w, y, flint_darken(flint_theme_get_bg(), 28));
}

static const char *
profile_tab_title(int tab)
{
    switch(tab) {
        case PROFILE_TAB_FRIENDS:
            return locale_get("profile_friends_title");
        case PROFILE_TAB_LEADERBOARD:
            return locale_get("profile_leaderboard_title");
        case PROFILE_TAB_DATA:
            return locale_get("profile_data_title");
        case PROFILE_TAB_OVERVIEW:
        default:
            return locale_get("tab_profile");
    }
}

static void
profile_overview_column(int *x, int *w)
{
    flint_centered_column(flint_px(CONTENT_MAX_W), flint_page_side_padding(), x, w);
}

static Rectangle
profile_guide_account_anchor(void)
{
    int x;
    int w;

    profile_overview_column(&x, &w);
    return (Rectangle){(float)(x - flint_px(6)), (float)flint_px(8),
                       (float)(w + flint_px(12)), (float)flint_px(70)};
}

static Rectangle
profile_guide_data_anchor(void)
{
    int x;
    int w;
    int y = flint_px(82);

    profile_overview_column(&x, &w);
    return (Rectangle){(float)(x - flint_px(6)), (float)(y - flint_px(6)),
                       (float)(w + flint_px(12)), (float)flint_px(190)};
}

static Rectangle
profile_guide_social_anchor(InbeApp *app)
{
    int x;
    int w;
    int y = flint_px(278);

    (void)app;
    profile_overview_column(&x, &w);
    return (Rectangle){(float)(x - flint_px(6)), (float)(y - flint_px(6)),
                       (float)(w + flint_px(12)), (float)flint_px(102)};
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
    return app != NULL && !app->profile_guide_seen && !app->modal.active &&
           app->inbe.screen == InbeScreenProfile;
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
    FlintUIGuideStep steps[PROFILE_GUIDE_STEPS];
    FlintUIGuideResult result;

    if(!profile_screen_first_run_guide_active(app))
        return;

    steps[0] = (FlintUIGuideStep){
        .anchor = profile_guide_account_anchor(),
        .text = locale_get("profile_guide_account")
    };
    steps[1] = (FlintUIGuideStep){
        .anchor = profile_guide_data_anchor(),
        .text = locale_get("profile_guide_data")
    };
    steps[2] = (FlintUIGuideStep){
        .anchor = profile_guide_social_anchor(app),
        .text = locale_get(sync_account_load(&(InbeSyncAccount){0})
                               ? "profile_guide_social"
                               : "profile_guide_social_no_account")
    };

    result = flint_ui_draw_guide_overlay((FlintUIGuideOverlay){
        .steps = steps,
        .count = PROFILE_GUIDE_STEPS,
        .step = &app->profile_guide_step,
        .view_width = view_width,
        .view_height = view_height,
        .reserved_top = 0,
        .reserved_bottom = ui_bottom_nav_height(),
        .max_width = flint_px(300),
        .paragraph_font = flint_ui_font_small(),
        .close_icon = app->icons[UI_ICON_TYPE_X],
        .back_icon = app->icons[UI_ICON_TYPE_BACKWARD],
        .next_icon = app->icons[UI_ICON_TYPE_FORWARD],
        .done_icon = app->icons[UI_ICON_TYPE_CHECK]
    });
    if(result.closed || result.finished)
        profile_screen_finish_first_run_guide(app);
}

static int
profile_json_array_count(const char *json, const char *array_key)
{
    char key_pattern[48];
    const char *p;
    const char *end;
    int count = 0;

    snprintf(key_pattern, sizeof(key_pattern), "\"%s\"", array_key);
    p = strstr(json != NULL ? json : "", key_pattern);
    if(p == NULL)
        return 0;
    p = strchr(p, '[');
    end = p != NULL ? strchr(p, ']') : NULL;
    if(p == NULL || end == NULL)
        return 0;
    while((p = strchr(p, '{')) != NULL && p < end) {
        count++;
        p++;
    }
    return count;
}

static void
profile_draw_summary_columns(int x, int w, int *y)
{
    char habits[32];
    char sessions[32];
    char storage[64];
    const char *labels[3] = {
        locale_get("profile_habits_label"),
        locale_get("profile_sessions_label"),
        locale_get("profile_storage_label")
    };
    const char *values[3] = {habits, sessions, storage};
    int col_w = w / 3;
    int value_font = FLINT_TEXT_16;
    int label_font = flint_ui_font_small();

    snprintf(habits, sizeof(habits), "%d", storage_habit_count());
    snprintf(sessions, sizeof(sessions), "%d", storage_session_count());
    profile_format_size(storage, sizeof(storage), storage_total_size());

    for(int i = 0; i < 3; i++) {
        int cx = x + i * col_w;
        int cw = i == 2 ? x + w - cx : col_w;
        flint_text_draw(values[i], cx, *y, value_font, flint_theme_get_text());
        flint_text_draw(labels[i], cx, *y + flint_px(24), label_font,
                        flint_darken(flint_theme_get_text(), 32));
        if(i < 2)
            DrawLine(cx + cw - flint_px(10), *y - flint_px(2),
                     cx + cw - flint_px(10), *y + flint_px(42),
                     flint_darken(flint_theme_get_bg(), 24));
    }
    *y += flint_px(58);
}

static void
profile_draw_overview(InbeApp *app, int x, int w, int *y)
{
    int font = flint_ui_font();
    int small = flint_ui_font_small();
    int btn_h = flint_px(34);
    int hover_account = 0;
    int hover_data = 0;
    int hover_habits = 0;
    int hover_practices = 0;
    int hover_friends = 0;
    int hover_leaderboard = 0;
    int half_w = (w - flint_px(8)) / 2;
    InbeSyncAccount account;
    int has_account;
    const char *alias;
    char account_text[96];
    char pending_text[64];
    char friend_text[64];

    if(!app->profile_friends_loaded)
        profile_load_friends_cache(app);

    *y += flint_px(14);
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
        snprintf(account_text, sizeof(account_text), "%s", locale_get("profile_no_account"));
    }

    flint_text_draw(locale_get("profile_account_section"), x, *y, small,
                    flint_darken(flint_theme_get_text(), 35));
    *y += flint_px(18);
    flint_text_draw(account_text, x, *y, font, flint_theme_get_text());
    if(ui_draw_generic_button(x + w - flint_px(132), *y - flint_px(8), flint_px(132),
                              btn_h, locale_get("profile_configure_button"),
                              UI_BUTTON_STYLE_SECONDARY, 0,
                              &hover_account))
        settings_data_open_sync_account_config(app);
    *y += flint_px(38);
    profile_draw_divider(x, w, *y);
    *y += flint_px(18);

    flint_text_draw(locale_get("profile_data_section"), x, *y, small,
                    flint_darken(flint_theme_get_text(), 35));
    *y += flint_px(24);
    profile_draw_summary_columns(x, w, y);
    if(ui_draw_generic_button(x, *y, w, btn_h, locale_get("profile_data_button"),
                              UI_BUTTON_STYLE_SECONDARY, 0, &hover_data)) {
        app->profile_tab = PROFILE_TAB_DATA;
        app->profile_scroll = 0;
        settings_screen_clear_status();
    }
    *y += btn_h + flint_px(8);
    if(ui_draw_generic_button(x, *y, half_w, btn_h, locale_get("profile_my_habits_button"),
                              UI_BUTTON_STYLE_SECONDARY, 0, &hover_habits)) {
        app->profile_view = PROFILE_VIEW_HABITS;
        app->profile_scroll = 0;
        settings_screen_clear_status();
    }
    if(ui_draw_generic_button(x + half_w + flint_px(8), *y, half_w, btn_h,
                              locale_get("profile_my_practices_button"),
                              UI_BUTTON_STYLE_SECONDARY, 0, &hover_practices)) {
        app->profile_view = PROFILE_VIEW_PRACTICES;
        app->profile_scroll = 0;
        settings_screen_clear_status();
    }
    *y += btn_h + flint_px(20);
    profile_draw_divider(x, w, *y);
    *y += flint_px(18);

    snprintf(friend_text, sizeof(friend_text), locale_get("profile_friends_count_format"),
             profile_json_array_count(app->profile_friends_json, "friends"));
    snprintf(pending_text, sizeof(pending_text), locale_get("profile_pending_count_format"),
             profile_json_array_count(app->profile_friend_requests_json, "incoming"));
    flint_text_draw(locale_get("profile_social_section"), x, *y, small,
                    flint_darken(flint_theme_get_text(), 35));
    *y += flint_px(20);
    flint_text_draw(friend_text, x, *y, font, flint_theme_get_text());
    flint_text_draw(pending_text, x + w - flint_text_measure(pending_text, font), *y,
                    font, flint_theme_get_text());
    *y += flint_px(34);
    if(ui_draw_generic_button(x, *y, half_w, btn_h, locale_get("profile_friends_title"),
                              UI_BUTTON_STYLE_PRIMARY, !has_account, &hover_friends)) {
        app->profile_tab = PROFILE_TAB_FRIENDS;
        app->profile_scroll = 0;
    }
    if(ui_draw_generic_button(x + half_w + flint_px(8), *y, half_w, btn_h,
                              locale_get("profile_leaderboard_title"), UI_BUTTON_STYLE_PRIMARY,
                              !has_account, &hover_leaderboard)) {
        app->profile_tab = PROFILE_TAB_LEADERBOARD;
        app->profile_scroll = 0;
    }
    *y += btn_h + flint_px(16);
}

static void
profile_refresh_friends(InbeApp *app)
{
    if(app == NULL)
        return;
    profile_load_friends_cache(app);
    app_request_social_refresh(app);
    settings_screen_set_status_success(locale_get("profile_updating_status"), NULL);
}

static void
profile_draw_friends(InbeApp *app, int x, int w, int *y)
{
    int font = flint_ui_font();
    int btn_h = flint_px(34);
    int hover_add = 0;
    int hover_refresh = 0;
    int commit = 0;
    char url[256];

    if(!app->profile_friends_loaded)
        profile_load_friends_cache(app);

    *y += flint_px(16);
    flint_text_draw(locale_get("profile_friends_title"), x, *y, font,
                    flint_theme_get_text());
    *y += flint_px(30);
    (void)flint_ui_text_field((FlintUITextField){
        .bounds = {x, *y, w, flint_px(38)},
        .text = app->profile_friend_input,
        .text_size = sizeof(app->profile_friend_input),
        .cursor_position = &app->profile_friend_input_cursor,
        .focused = &app->profile_friend_input_focused,
        .max_codepoints = 79,
        .font = flint_ui_font(),
        .focus_id = 810,
        .style = profile_text_style(),
        .filter = profile_friend_filter,
        .commit_pressed = &commit
    });
    *y += flint_px(46);
    if((ui_draw_generic_button(x, *y, w, btn_h,
                               locale_get("profile_add_friend_button"),
                               UI_BUTTON_STYLE_PRIMARY,
                               app->profile_friend_input[0] == '\0', &hover_add) || commit) &&
       app->profile_friend_input[0] != '\0') {
        if(profile_sync_url(url, sizeof(url))) {
            app_request_friend_send(app, app->profile_friend_input);
            settings_screen_set_status_success(locale_get("profile_updating_status"), NULL);
            profile_load_friends_cache(app);
            app->profile_friend_input[0] = '\0';
            app->profile_friend_input_cursor = 0;
        } else {
            settings_screen_set_status_error(locale_get("sync_server_url_invalid"));
        }
    }
    *y += btn_h + flint_px(10);
    if(ui_draw_generic_button(x, *y, w, btn_h, locale_get("profile_refresh_button"),
                              UI_BUTTON_STYLE_SECONDARY,
                              0, &hover_refresh)) {
        profile_refresh_friends(app);
    }
    *y += btn_h + flint_px(22);

    flint_text_draw(locale_get("profile_incoming_title"), x, *y, font,
                    flint_theme_get_text());
    *y += flint_px(28);
    profile_draw_json_people(app->profile_friend_requests_json, "incoming", x, w, y, 1, app);
    *y += flint_px(16);
    flint_text_draw(locale_get("profile_friends_title"), x, *y, font,
                    flint_theme_get_text());
    *y += flint_px(28);
    profile_draw_json_people(app->profile_friends_json, "friends", x, w, y, 0, app);
    *y += flint_px(16);
    flint_text_draw(locale_get("profile_outgoing_title"), x, *y, font,
                    flint_theme_get_text());
    *y += flint_px(28);
    profile_draw_json_people(app->profile_friend_requests_json, "outgoing", x, w, y, 0, app);
}

static void
profile_draw_leaderboard_value(int x, int w, int y, const char *value, int font)
{
    int rank_w = flint_text_measure(value, font);
    flint_text_draw(value, x + w - rank_w, y, font, flint_theme_get_text());
}

static void
profile_leaderboard_display_name(char *out, size_t out_size,
                                 const char *alias, const char *user_id)
{
    int written;

    if(out == NULL || out_size == 0)
        return;
    if(alias != NULL && alias[0] != '\0') {
        if(out_size <= 2) {
            snprintf(out, out_size, "@");
            return;
        }
        written = snprintf(out, out_size, "@%.*s", (int)out_size - 2, alias);
        if(written < 0)
            out[0] = '\0';
    } else if(user_id != NULL && strlen(user_id) > 12) {
        snprintf(out, out_size, "%.12s...", user_id);
    } else {
        snprintf(out, out_size, "%.*s", (int)out_size - 1,
                 user_id != NULL ? user_id : "");
    }
}

static int
profile_today_index(void)
{
    time_t now = time(NULL);
    struct tm *tm_value = localtime(&now);

    if(tm_value == NULL)
        return 0;
    return (tm_value->tm_year + 1900) * 10000 +
           (tm_value->tm_mon + 1) * 100 + tm_value->tm_mday;
}

static double
profile_local_leaderboard_value(InbeApp *app, char *out, size_t out_size)
{
    int streak = 0;
    long avg = 0;

    if(out == NULL || out_size == 0)
        return 0.0;
    out[0] = '\0';
    if(app == NULL)
        return 0.0;
    storage_profile_activity_stats(app->profile_leaderboard_practice,
                                   profile_today_index(), &streak, &avg);
    if(app->profile_leaderboard_metric != PROFILE_LEADERBOARD_AVG_HOLD) {
        snprintf(out, out_size, "%ld", (long)streak);
        return (double)streak;
    } else if(app->profile_leaderboard_practice == EXERCISE_MEDITATION) {
        profile_format_leaderboard_time(avg, out, out_size);
        return (double)avg;
    } else {
        snprintf(out, out_size, "%ld", avg);
        return (double)avg;
    }
}

typedef struct ProfileLeaderboardDrawRow {
    char user_id[80];
    char alias[48];
    char value[48];
    double sort_value;
} ProfileLeaderboardDrawRow;

static int
profile_leaderboard_row_seen(const ProfileLeaderboardDrawRow *rows, int count,
                             const char *user_id)
{
    if(user_id == NULL || user_id[0] == '\0')
        return 1;
    for(int i = 0; i < count; i++) {
        if(strcmp(rows[i].user_id, user_id) == 0)
            return 1;
    }
    return 0;
}

static int
profile_leaderboard_row_compare(const void *left_ptr, const void *right_ptr)
{
    const ProfileLeaderboardDrawRow *left = left_ptr;
    const ProfileLeaderboardDrawRow *right = right_ptr;
    const char *left_name = left->alias[0] != '\0' ? left->alias : left->user_id;
    const char *right_name = right->alias[0] != '\0' ? right->alias : right->user_id;

    if(left->sort_value < right->sort_value)
        return 1;
    if(left->sort_value > right->sort_value)
        return -1;
    return strcmp(left_name, right_name);
}

static void
profile_draw_leaderboard_rows(InbeApp *app, const char *json, int x, int w, int *y)
{
    const char *p = strstr(json != NULL ? json : "", "\"rows\"");
    int rows = 0;
    int font = flint_ui_font();
    int small = flint_ui_font_small();
    ProfileLeaderboardDrawRow draw_rows[24];
    InbeSyncAccount account;
    int has_account = sync_account_load(&account) && account.public_id[0] != '\0';
    int self_seen = 0;
    char self_value[48];
    double self_sort_value;

    self_sort_value = profile_local_leaderboard_value(app, self_value, sizeof(self_value));

    while(p != NULL && (p = strchr(p, '{')) != NULL && rows < 24) {
        char alias[48];
        char user_id[80];
        char value[48];
        char label[48];
        double number;

        if(profile_json_string_value(p, "user_id_hash", user_id, sizeof(user_id)) == NULL)
            break;
        profile_json_string_value(p, "alias", alias, sizeof(alias));
        profile_json_string_value(p, "label", label, sizeof(label));
        number = profile_json_number_value(p, "value");
        if(has_account && strcmp(user_id, account.public_id) == 0) {
            snprintf(value, sizeof(value), "%s", self_value);
            number = self_sort_value;
            self_seen = 1;
        } else if(app != NULL &&
                  app->profile_leaderboard_practice == EXERCISE_MEDITATION &&
                  app->profile_leaderboard_metric == PROFILE_LEADERBOARD_AVG_HOLD &&
                  label[0] != '\0') {
            snprintf(value, sizeof(value), "%s", label);
        } else {
            snprintf(value, sizeof(value), "%.0f", number);
        }
        snprintf(draw_rows[rows].user_id, sizeof(draw_rows[rows].user_id), "%s", user_id);
        snprintf(draw_rows[rows].alias, sizeof(draw_rows[rows].alias), "%s", alias);
        snprintf(draw_rows[rows].value, sizeof(draw_rows[rows].value), "%s", value);
        draw_rows[rows].sort_value = number;
        rows++;
        p++;
    }

    if(has_account && !self_seen && rows < 24) {
        const char *alias_text = storage_get_setting_text("sync_account_alias");

        snprintf(draw_rows[rows].user_id, sizeof(draw_rows[rows].user_id), "%s",
                 account.public_id);
        snprintf(draw_rows[rows].alias, sizeof(draw_rows[rows].alias), "%s",
                 alias_text != NULL ? alias_text : "");
        snprintf(draw_rows[rows].value, sizeof(draw_rows[rows].value), "%s", self_value);
        draw_rows[rows].sort_value = self_sort_value;
        rows++;
    }

    p = strstr(app != NULL ? app->profile_friends_json : "", "\"friends\"");
    while(p != NULL && (p = strchr(p, '{')) != NULL && rows < 24) {
        char alias[48];
        char user_id[80];

        if(profile_json_string_value(p, "user_id_hash", user_id, sizeof(user_id)) == NULL)
            break;
        if(profile_leaderboard_row_seen(draw_rows, rows, user_id)) {
            p++;
            continue;
        }
        profile_json_string_value(p, "alias", alias, sizeof(alias));
        snprintf(draw_rows[rows].user_id, sizeof(draw_rows[rows].user_id), "%s", user_id);
        snprintf(draw_rows[rows].alias, sizeof(draw_rows[rows].alias), "%s", alias);
        snprintf(draw_rows[rows].value, sizeof(draw_rows[rows].value), "%s",
                 app != NULL &&
                 app->profile_leaderboard_practice == EXERCISE_MEDITATION &&
                 app->profile_leaderboard_metric == PROFILE_LEADERBOARD_AVG_HOLD
                     ? "0:00"
                     : "0");
        draw_rows[rows].sort_value = 0.0;
        rows++;
        p++;
    }

    qsort(draw_rows, (size_t)rows, sizeof(draw_rows[0]), profile_leaderboard_row_compare);
    for(int i = 0; i < rows && i < 12; i++) {
        char display[96];
        char name[128];

        profile_leaderboard_display_name(display, sizeof(display),
                                         draw_rows[i].alias, draw_rows[i].user_id);
        snprintf(name, sizeof(name), "%d. %s", i + 1, display);
        flint_text_draw(name, x, *y, font, flint_theme_get_text());
        profile_draw_leaderboard_value(x, w, *y, draw_rows[i].value, font);
        *y += flint_px(30);
    }

    if(rows == 0) {
        flint_text_draw(locale_get("profile_no_leaderboard_data"), x, *y, small,
                        flint_darken(flint_theme_get_text(), 35));
        *y += flint_px(24);
    }
}

static void
profile_refresh_leaderboard(InbeApp *app)
{
    if(app == NULL)
        return;
    profile_load_leaderboard_cache(app);
    app_request_social_refresh(app);
    settings_screen_set_status_success(locale_get("profile_updating_status"), NULL);
}

void
profile_screen_refresh_social_cache(InbeApp *app)
{
    if(app == NULL)
        return;
    profile_refresh_friends(app);
    profile_refresh_leaderboard(app);
}

static void
profile_draw_choice_row(InbeApp *app, int x, int w, int *y, int *value,
                        const char *const *labels, int count)
{
    int gap = flint_px(8);
    int btn_h = flint_px(34);
    int btn_w;

    if(count <= 0)
        return;
    btn_w = (w - gap * (count - 1)) / count;
    for(int i = 0; i < count; i++) {
        int hover = 0;
        int bx = x + i * (btn_w + gap);
        int bw = i == count - 1 ? x + w - bx : btn_w;
        int selected = *value == i;
        if(ui_draw_generic_button(bx, *y, bw, btn_h, labels[i],
                                  selected ? UI_BUTTON_STYLE_TAB_SELECTED
                                           : UI_BUTTON_STYLE_TAB,
                                  0, &hover) && !selected) {
            *value = i;
            profile_load_leaderboard_cache(app);
        }
    }
    *y += btn_h + flint_px(10);
}

static void
profile_draw_leaderboard(InbeApp *app, int x, int w, int *y)
{
    const char *practice_options[EXERCISE_COUNT];
    const char *metric_options[PROFILE_LEADERBOARD_METRIC_COUNT];
    int metric_count;
    int before_practice;
    int before_metric;

    for(int i = 0; i < EXERCISE_COUNT; i++)
        practice_options[i] = profile_practice_label(i);
    if(app->profile_leaderboard_practice < 0 ||
       app->profile_leaderboard_practice >= EXERCISE_COUNT)
        app->profile_leaderboard_practice = EXERCISE_WIM_HOF;
    if(app->profile_leaderboard_metric < 0 ||
       app->profile_leaderboard_metric >= PROFILE_LEADERBOARD_METRIC_COUNT)
        app->profile_leaderboard_metric = PROFILE_LEADERBOARD_STREAK;
    metric_count = profile_leaderboard_metric_count(app->profile_leaderboard_practice);
    if(app->profile_leaderboard_metric >= metric_count)
        app->profile_leaderboard_metric = PROFILE_LEADERBOARD_STREAK;
    if(!app->profile_leaderboard_loaded)
        profile_load_leaderboard_cache(app);
    metric_options[PROFILE_LEADERBOARD_STREAK] =
        profile_leaderboard_metric_label(app->profile_leaderboard_practice,
                                         PROFILE_LEADERBOARD_STREAK);
    metric_options[PROFILE_LEADERBOARD_AVG_HOLD] =
        profile_leaderboard_metric_label(app->profile_leaderboard_practice,
                                         PROFILE_LEADERBOARD_AVG_HOLD);

    *y += flint_px(16);
    flint_text_draw(locale_get("profile_practice_label"), x, *y, flint_ui_font_small(),
                    flint_darken(flint_theme_get_text(), 35));
    *y += flint_px(22);
    before_practice = app->profile_leaderboard_practice;
    ui_draw_dropdown_button(811, x, *y, w, flint_px(36), practice_options,
                            EXERCISE_COUNT, &app->profile_leaderboard_practice);
    if(app->profile_leaderboard_practice != before_practice) {
        metric_count = profile_leaderboard_metric_count(app->profile_leaderboard_practice);
        if(app->profile_leaderboard_metric >= metric_count)
            app->profile_leaderboard_metric = PROFILE_LEADERBOARD_STREAK;
        metric_options[PROFILE_LEADERBOARD_STREAK] =
            profile_leaderboard_metric_label(app->profile_leaderboard_practice,
                                             PROFILE_LEADERBOARD_STREAK);
        metric_options[PROFILE_LEADERBOARD_AVG_HOLD] =
            profile_leaderboard_metric_label(app->profile_leaderboard_practice,
                                             PROFILE_LEADERBOARD_AVG_HOLD);
        profile_refresh_leaderboard(app);
    }
    *y += flint_px(46);
    before_metric = app->profile_leaderboard_metric;
    profile_draw_choice_row(app, x, w, y, &app->profile_leaderboard_metric,
                            metric_options, metric_count);
    if(app->profile_leaderboard_metric != before_metric)
        profile_refresh_leaderboard(app);
    *y += flint_px(22);
    if(!app->profile_friends_loaded)
        profile_load_friends_cache(app);
    profile_draw_leaderboard_rows(app, app->profile_leaderboard_json, x, w, y);
}

static void
profile_draw_data(InbeApp *app, int x, int w, int *y)
{
    int font = flint_ui_font();

    *y += flint_px(12);
    flint_text_draw(locale_get("profile_data_title"), x, *y, font, flint_theme_get_text());
    *y += flint_px(28);
    profile_draw_data_summary(x, w, y);
    settings_data_draw_sync_status(x, w, y);
    *y += flint_px(10);
    settings_data_draw_actions(app, x, w, y);
}

static void
profile_draw_habits(InbeApp *app, int x, int w, int *y)
{
    int font = flint_ui_font();
    int small = flint_ui_font_small();
    int btn_h = flint_px(32);
    int btn_w = flint_px(74);
    int gap = flint_px(8);

    *y += flint_px(16);
    flint_text_draw(locale_get("profile_my_habits_title"), x, *y, font,
                    flint_theme_get_text());
    *y += flint_px(34);

    for(int i = 0; i < app->habits.count; i++) {
        int row_y = *y;
        int up_hover = 0;
        int down_hover = 0;

        flint_text_draw(app->habits.items[i].name, x, row_y + flint_px(7),
                        font, flint_theme_get_text());
        if(ui_draw_generic_button(x + w - btn_w * 2 - gap, row_y, btn_w, btn_h,
                                  locale_get("move_up_button"),
                                  UI_BUTTON_STYLE_SECONDARY, i == 0, &up_hover)) {
            if(habits_move(&app->habits, i, i - 1))
                app_auto_sync(app);
            return;
        }
        if(ui_draw_generic_button(x + w - btn_w, row_y, btn_w, btn_h,
                                  locale_get("move_down_button"),
                                  UI_BUTTON_STYLE_SECONDARY,
                                  i == app->habits.count - 1, &down_hover)) {
            if(habits_move(&app->habits, i, i + 1))
                app_auto_sync(app);
            return;
        }
        *y += flint_px(46);
        profile_draw_divider(x, w, *y - flint_px(6));
    }

    if(app->habits.count <= 0)
        flint_text_draw(locale_get("habit_empty_title"), x, *y, small,
                        flint_darken(flint_theme_get_text(), 35));
}

static void
profile_draw_practices(InbeApp *app, int x, int w, int *y)
{
    int font = flint_ui_font();

    (void)w;
    *y += flint_px(16);
    flint_text_draw(locale_get("profile_my_practices_title"), x, *y, font,
                    flint_theme_get_text());
    *y += flint_px(34);

    for(int i = 0; i < EXERCISE_COUNT; i++) {
        int enabled = practice_is_visible(app, i);
        if(ui_draw_checkbox_row((FlintUICheckboxRow){
            .label = profile_practice_label(i),
            .value = &enabled
        }, x, *y)) {
            practice_set_visible(app, i, enabled);
        }
        *y += ui_checkbox_row_height((FlintUICheckboxRow){0});
    }
}

int
profile_screen_draw(InbeApp *app)
{
    int is_profile_subpage = app->profile_view == PROFILE_VIEW_MAIN &&
                             app->profile_tab != PROFILE_TAB_OVERVIEW;
    int header_h = (app->profile_view != PROFILE_VIEW_MAIN || is_profile_subpage)
                       ? flint_px(58)
                       : 0;
    int content_y = header_h;
    int content_h = view_height - content_y - app_content_bottom_reserved(app);
    FlintUIScrollPage page;
    int y;

#if ANDROID_BUILD
    settings_data_handle_android_import(app);
#elif defined(PLATFORM_WEB)
    settings_data_handle_web_import(app);
#endif

    if(settings_data_draw_pending_file_dialog(app))
        return 1;

    if(app->profile_tab < 0 || app->profile_tab >= PROFILE_TAB_COUNT)
        app->profile_tab = PROFILE_TAB_OVERVIEW;
    if((app->profile_tab == PROFILE_TAB_FRIENDS ||
        app->profile_tab == PROFILE_TAB_LEADERBOARD) &&
       !sync_account_load(&(InbeSyncAccount){0})) {
        app->profile_tab = PROFILE_TAB_OVERVIEW;
        app->profile_scroll = 0;
    }

    if(app->profile_view != PROFILE_VIEW_MAIN) {
        const char *title = locale_get("profile_data_title");
        if(app->profile_view == PROFILE_VIEW_SYNC_ACCOUNT)
            title = locale_get("sync_configure_account_button");
        else if(app->profile_view == PROFILE_VIEW_HABITS)
            title = locale_get("profile_my_habits_title");
        else if(app->profile_view == PROFILE_VIEW_PRACTICES)
            title = locale_get("profile_my_practices_title");
        FlintUIHeader header = ui_draw_title_header(header_h, title,
                                                    app->icons[UI_ICON_TYPE_RETURN],
                                                    (Texture2D){0});
        if(header.left_clicked) {
            app->profile_view = PROFILE_VIEW_MAIN;
            app->profile_scroll = 0;
            app->sync_server_url_focused = 0;
            settings_screen_clear_status();
        }
    } else if(is_profile_subpage) {
        FlintUIHeader header = ui_draw_title_header(header_h,
                                                    profile_tab_title(app->profile_tab),
                                                    app->icons[UI_ICON_TYPE_RETURN],
                                                    (Texture2D){0});
        if(header.left_clicked) {
            app->profile_tab = PROFILE_TAB_OVERVIEW;
            app->profile_scroll = 0;
            settings_screen_clear_status();
        }
    }

    if(content_h < 0)
        content_h = 0;

    page = ui_scroll_page_begin((FlintUIScrollPageSpec){
        .y = content_y,
        .height = content_h,
        .max_content_width = flint_px(CONTENT_MAX_W),
        .min_content_width = flint_px(320),
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
        profile_draw_friends(app, page.content_x, page.content_w, &y);
    else if(app->profile_tab == PROFILE_TAB_LEADERBOARD)
        profile_draw_leaderboard(app, page.content_x, page.content_w, &y);
    else if(app->profile_tab == PROFILE_TAB_DATA)
        profile_draw_data(app, page.content_x, page.content_w, &y);
    else
        profile_draw_overview(app, page.content_x, page.content_w, &y);

    ui_scroll_page_end(page);
    if(app->profile_view == PROFILE_VIEW_MAIN &&
       app->profile_tab == PROFILE_TAB_LEADERBOARD) {
        ui_set_dropdown_clip_top(content_y);
        ui_set_dropdown_clip_bottom(view_height - app_content_bottom_reserved(app));
        if(ui_draw_dropdown_menu(811)) {
            profile_load_leaderboard_cache(app);
            app->profile_scroll = 0;
        }
        ui_set_dropdown_clip_top(0);
        ui_set_dropdown_clip_bottom(0);
    }
    return 0;
}
