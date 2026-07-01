#include "profile_social.h"

#include "app.h"
#include "flint_locale.h"
#include "flint_theme.h"
#include "flint_ui.h"
#include "settings/settings_screen.h"
#include "storage.h"
#include "sync_account.h"
#include "sync_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define INBE_SYNC_SERVER_URL_KEY "sync_server_url"

enum {
    PROFILE_LEADERBOARD_STREAK = 0,
    PROFILE_LEADERBOARD_AVG_HOLD,
    PROFILE_LEADERBOARD_METRIC_COUNT
};

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

void
profile_social_load_friends_cache(InbeApp *app)
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

void
profile_social_load_leaderboard_cache(InbeApp *app)
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

int
profile_social_friends_count(InbeApp *app)
{
    if(app != NULL && !app->profile_friends_loaded)
        profile_social_load_friends_cache(app);
    return profile_json_array_count(app != NULL ? app->profile_friends_json : "",
                                    "friends");
}

int
profile_social_pending_count(InbeApp *app)
{
    if(app != NULL && !app->profile_friends_loaded)
        profile_social_load_friends_cache(app);
    return profile_json_array_count(app != NULL ? app->profile_friend_requests_json : "",
                                    "incoming");
}

static void
profile_public_id_short(char *out, size_t out_size, const char *user_id)
{
    if(out == NULL || out_size == 0)
        return;
    if(user_id == NULL || user_id[0] == '\0')
        snprintf(out, out_size, "----");
    else
        snprintf(out, out_size, "%.*s", 4, user_id);
}

static void
profile_display_name(char *out, size_t out_size, const char *alias,
                     const char *user_id)
{
    char short_id[8];

    if(out == NULL || out_size == 0)
        return;
    if(alias != NULL && alias[0] != '\0') {
        snprintf(out, out_size, "@%s", alias);
    } else if(user_id != NULL && user_id[0] != '\0') {
        profile_public_id_short(short_id, sizeof(short_id), user_id);
        snprintf(out, out_size, "%s...", short_id);
    } else {
        snprintf(out, out_size, "----...");
    }
}

static void
profile_prompt_remove_friend(InbeApp *app, const char *friend_user_id,
                             const char *friend_name)
{
    if(app == NULL || friend_user_id == NULL || friend_user_id[0] == '\0')
        return;
    snprintf(app->profile_pending_friend_remove_id,
             sizeof(app->profile_pending_friend_remove_id), "%s", friend_user_id);
    snprintf(app->profile_pending_friend_remove_name,
             sizeof(app->profile_pending_friend_remove_name), "%s",
             friend_name != NULL && friend_name[0] != '\0' ? friend_name : friend_user_id);
    app_open_modal(app, UIModalConfirmRemoveFriend);
}

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
    int with_remove = !with_actions && strcmp(array_key, "friends") == 0;
    int with_cancel = !with_actions && strcmp(array_key, "outgoing") == 0;

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
        int hover_remove = 0;
        int hover_cancel = 0;
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
        profile_display_name(title, sizeof(title), alias, display_id);
        flint_text_draw(title, x, *y, font, flint_theme_get_text());
        if(with_remove) {
            int icon_size = flint_px(18);
            int icon_padding = flint_px(6);
            int button_w = icon_size + icon_padding * 2;
            if(ui_draw_icon_btn_padded(x + w - button_w, *y - flint_px(6),
                                       icon_size, icon_padding,
                                       app->icons[UI_ICON_TYPE_TRASH],
                                       &hover_remove)) {
                profile_prompt_remove_friend(app, display_id, title);
            }
        } else if(with_cancel) {
            int icon_size = flint_px(18);
            int icon_padding = flint_px(6);
            int button_w = icon_size + icon_padding * 2;
            if(ui_draw_icon_btn_padded(x + w - button_w, *y - flint_px(6),
                                       icon_size, icon_padding,
                                       app->icons[UI_ICON_TYPE_TRASH],
                                       &hover_cancel) &&
               profile_sync_url(url, sizeof(url))) {
                app_request_friend_decline(app, id);
                settings_screen_set_status_success(locale_get("profile_updating_status"),
                                                   NULL);
            }
        }
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

void
profile_screen_draw_remove_friend_modal(InbeApp *app)
{
    char message[384];
    int modal_result;

    if(app == NULL || !app->modal.active ||
       app->modal.type != UIModalConfirmRemoveFriend)
        return;

    snprintf(message, sizeof(message),
             locale_get("profile_friend_remove_message"),
             app->profile_pending_friend_remove_name[0] != '\0'
                 ? app->profile_pending_friend_remove_name
                 : "this friend");
    modal_result = ui_draw_modal(locale_get("profile_friend_remove_title"), message,
                                 locale_get("cancel_button"),
                                 locale_get("delete_button"));
    if(modal_result == 1) {
        app->profile_pending_friend_remove_id[0] = '\0';
        app->profile_pending_friend_remove_name[0] = '\0';
        app_close_modal(app);
    } else if(modal_result == 2) {
        char url[256];

        if(profile_sync_url(url, sizeof(url))) {
            app_request_friend_remove(app, app->profile_pending_friend_remove_id);
            settings_screen_set_status_success(locale_get("profile_updating_status"),
                                               NULL);
        } else {
            settings_screen_set_status_error(locale_get("sync_server_url_invalid"));
        }
        app->profile_pending_friend_remove_id[0] = '\0';
        app->profile_pending_friend_remove_name[0] = '\0';
        app_close_modal(app);
    }
}

void
profile_social_draw_friends(InbeApp *app, int x, int w, int *y)
{
    int font = flint_ui_font();
    int btn_h = flint_px(34);
    int hover_add = 0;
    int commit = 0;
    char url[256];
    int incoming_count;
    int outgoing_count;

    if(app == NULL)
        return;
    if(!app->profile_friends_loaded)
        profile_social_load_friends_cache(app);
    incoming_count = profile_json_array_count(app->profile_friend_requests_json, "incoming");
    outgoing_count = profile_json_array_count(app->profile_friend_requests_json, "outgoing");

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
            profile_social_load_friends_cache(app);
            app->profile_friend_input[0] = '\0';
            app->profile_friend_input_cursor = 0;
        } else {
            settings_screen_set_status_error(locale_get("sync_server_url_invalid"));
        }
    }
    *y += btn_h + flint_px(22);

    if(incoming_count > 0) {
        flint_text_draw(locale_get("profile_incoming_title"), x, *y, font,
                        flint_theme_get_text());
        *y += flint_px(28);
        profile_draw_json_people(app->profile_friend_requests_json, "incoming", x, w, y, 1, app);
        *y += flint_px(16);
    }
    flint_text_draw(locale_get("profile_friends_title"), x, *y, font,
                    flint_theme_get_text());
    *y += flint_px(28);
    profile_draw_json_people(app->profile_friends_json, "friends", x, w, y, 0, app);
    if(outgoing_count > 0) {
        *y += flint_px(16);
        flint_text_draw(locale_get("profile_outgoing_title"), x, *y, font,
                        flint_theme_get_text());
        *y += flint_px(28);
        profile_draw_json_people(app->profile_friend_requests_json, "outgoing", x, w, y, 0, app);
    }
}

int
profile_social_friends_content_height(InbeApp *app, int content_w)
{
    int h = flint_px(16) + flint_px(30) + flint_px(46) +
            flint_px(34) + flint_px(22);
    int incoming_count;
    int outgoing_count;
    int friends_count;

    (void)content_w;
    if(app == NULL)
        return h;
    incoming_count = profile_json_array_count(app->profile_friend_requests_json, "incoming");
    outgoing_count = profile_json_array_count(app->profile_friend_requests_json, "outgoing");
    friends_count = profile_json_array_count(app->profile_friends_json, "friends");
    if(incoming_count > 0)
        h += flint_px(28) +
             (incoming_count > 8 ? 8 : incoming_count) * flint_px(42) +
             flint_px(16);
    h += flint_px(28) +
         (friends_count > 0 ? (friends_count > 8 ? 8 : friends_count) * flint_px(30)
                            : flint_px(24));
    if(outgoing_count > 0)
        h += flint_px(16) + flint_px(28) +
             (outgoing_count > 8 ? 8 : outgoing_count) * flint_px(30);
    return h + flint_px(16);
}

static void
profile_draw_leaderboard_value(int x, int w, int y, const char *value, int font)
{
    int rank_w = flint_text_measure(value, font);
    flint_text_draw(value, x + w - rank_w, y, font, flint_theme_get_text());
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

        profile_display_name(display, sizeof(display),
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

static int
profile_leaderboard_row_count(InbeApp *app, const char *json)
{
    const char *p = strstr(json != NULL ? json : "", "\"rows\"");
    ProfileLeaderboardDrawRow draw_rows[24];
    InbeSyncAccount account;
    int has_account = sync_account_load(&account) && account.public_id[0] != '\0';
    int self_seen = 0;
    int rows = 0;

    while(p != NULL && (p = strchr(p, '{')) != NULL && rows < 24) {
        char user_id[80];

        if(profile_json_string_value(p, "user_id_hash", user_id, sizeof(user_id)) == NULL)
            break;
        if(has_account && strcmp(user_id, account.public_id) == 0)
            self_seen = 1;
        snprintf(draw_rows[rows].user_id, sizeof(draw_rows[rows].user_id), "%s", user_id);
        draw_rows[rows].alias[0] = '\0';
        rows++;
        p++;
    }
    if(has_account && !self_seen && rows < 24) {
        snprintf(draw_rows[rows].user_id, sizeof(draw_rows[rows].user_id), "%s",
                 account.public_id);
        rows++;
    }
    p = strstr(app != NULL ? app->profile_friends_json : "", "\"friends\"");
    while(p != NULL && (p = strchr(p, '{')) != NULL && rows < 24) {
        char user_id[80];

        if(profile_json_string_value(p, "user_id_hash", user_id, sizeof(user_id)) == NULL)
            break;
        if(!profile_leaderboard_row_seen(draw_rows, rows, user_id)) {
            snprintf(draw_rows[rows].user_id, sizeof(draw_rows[rows].user_id), "%s",
                     user_id);
            rows++;
        }
        p++;
    }
    return rows > 12 ? 12 : rows;
}

int
profile_social_leaderboard_content_height(InbeApp *app, int content_w)
{
    int metric_count;
    int rows;

    (void)content_w;
    if(app == NULL)
        return flint_px(160);
    metric_count = profile_leaderboard_metric_count(app->profile_leaderboard_practice);
    rows = profile_leaderboard_row_count(app, app->profile_leaderboard_json);
    return flint_px(16) + flint_px(22) + flint_px(46) +
           flint_px(34) + flint_px(10) +
           (metric_count > 0 ? 0 : 0) +
           flint_px(22) +
           (rows > 0 ? rows * flint_px(30) : flint_px(24)) +
           flint_px(16);
}

static void
profile_refresh_leaderboard(InbeApp *app)
{
    if(app == NULL)
        return;
    profile_social_load_leaderboard_cache(app);
    app_request_social_refresh(app);
    settings_screen_set_status_success(locale_get("profile_updating_status"), NULL);
}

void
profile_social_refresh_cache(InbeApp *app)
{
    if(app == NULL)
        return;
    profile_social_load_friends_cache(app);
    profile_social_load_leaderboard_cache(app);
    app_request_social_refresh(app);
    settings_screen_set_status_success(locale_get("profile_updating_status"), NULL);
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
            profile_social_load_leaderboard_cache(app);
        }
    }
    *y += btn_h + flint_px(10);
}

void
profile_social_draw_leaderboard(InbeApp *app, int x, int w, int *y)
{
    const char *practice_options[EXERCISE_COUNT];
    const char *metric_options[PROFILE_LEADERBOARD_METRIC_COUNT];
    int metric_count;
    int before_practice;
    int before_metric;

    if(app == NULL)
        return;
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
        profile_social_load_leaderboard_cache(app);
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
        profile_social_load_friends_cache(app);
    profile_draw_leaderboard_rows(app, app->profile_leaderboard_json, x, w, y);
}
