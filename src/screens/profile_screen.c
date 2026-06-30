#include "profile_screen.h"

#include "app.h"
#include "text_utils.h"
#include "flint_locale.h"
#include "flint_theme.h"
#include "flint_ui.h"
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
#include <time.h>

extern int view_height;
extern int view_width;

#define INBE_SYNC_SERVER_URL_KEY "sync_server_url"

enum {
    PROFILE_GUIDE_STEPS = 3
};

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
    int row_h = content_w < flint_px(260) ? flint_px(78) : flint_px(48);

    return flint_px(90) + row_h * INBE_HABIT_MAX;
}

static int
profile_habits_overview_label_width(int content_w)
{
    int min_label_w = flint_px(52);
    int max_label_w = flint_px(128);
    int gap = flint_px(4);
    int cell = flint_px(26);
    int label_w = content_w - gap * 7 - cell * 7;

    if(label_w < min_label_w)
        label_w = min_label_w;
    if(label_w > max_label_w)
        label_w = max_label_w;
    return label_w;
}

static int
profile_habits_overview_cell_size(int content_w)
{
    int label_w = profile_habits_overview_label_width(content_w);
    int gap = flint_px(4);
    int cell = (content_w - label_w - gap * 7) / 7;

    if(cell > flint_px(26))
        cell = flint_px(26);
    if(cell < flint_px(16))
        cell = flint_px(16);
    return cell;
}

static int
profile_habits_overview_stack_header(int content_w)
{
    return content_w < flint_px(300);
}

static int
profile_habits_overview_height(const InbeApp *app, int content_w)
{
    int count = app != NULL ? app->habits.count : 0;
    int rows = count > 0 ? count : 1;
    int cell = profile_habits_overview_cell_size(content_w);
    int header_h = flint_px(32) + flint_px(8);

    if(profile_habits_overview_stack_header(content_w))
        header_h += flint_px(38);
    return header_h + flint_px(20) + rows * (cell + flint_px(6)) +
           flint_px(18);
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
    return profile_main_content_height(content_w) +
           profile_habits_overview_height(app, content_w);
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
profile_guide_data_anchor(InbeApp *app)
{
    int x;
    int w;

    profile_overview_column(&x, &w);
    return (Rectangle){(float)(x - flint_px(6)),
                       (float)(flint_px(82) +
                               profile_habits_overview_height(app, w) - flint_px(6)),
                       (float)(w + flint_px(12)), (float)flint_px(190)};
}

static Rectangle
profile_guide_social_anchor(InbeApp *app)
{
    int x;
    int w;

    profile_overview_column(&x, &w);
    return (Rectangle){(float)(x - flint_px(6)),
                       (float)(flint_px(278) +
                               profile_habits_overview_height(app, w) - flint_px(6)),
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
        .anchor = profile_guide_data_anchor(app),
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
profile_draw_fitted_text(const char *text, int x, int y, int max_w, int font, Color color)
{
    char fitted[INBE_HABIT_NAME_SIZE + 4];

    if(text == NULL || max_w <= 0)
        return;
    inbe_text_fit_ellipsis(text, fitted, sizeof(fitted), max_w, font);
    flint_text_draw(fitted, x, y, font, color);
}

static int
profile_week_day_index(int offset)
{
    time_t now = time(NULL);
    struct tm week;
    struct tm *local = localtime(&now);
    int sunday_delta;

    if(local == NULL)
        return habits_today_index();
    week = *local;
    sunday_delta = week.tm_wday;
    week.tm_hour = 12;
    week.tm_min = 0;
    week.tm_sec = 0;
    week.tm_mday += offset - sunday_delta;
    if(mktime(&week) == (time_t)-1)
        return habits_today_index();
    return (week.tm_year + 1900) * 10000 + (week.tm_mon + 1) * 100 +
           week.tm_mday;
}

static void
profile_draw_habits_overview(InbeApp *app, int x, int w, int *y)
{
    int font = flint_ui_font();
    int small = flint_ui_font_small();
    int btn_h = flint_px(32);
    int btn_w = flint_px(132);
    int label_w = profile_habits_overview_label_width(w);
    int gap = flint_px(8);
    int cell_gap = flint_px(4);
    int cell = profile_habits_overview_cell_size(w);
    int grid_total_w = label_w + cell_gap * 7 + cell * 7;
    int overview_x = x + (w - grid_total_w) / 2;
    int grid_x = overview_x + label_w;
    int grid_y;
    int today_index = habits_today_index();
    int completed_count = 0;
    int manage_hover = 0;
    int stack_header = profile_habits_overview_stack_header(w);
    char progress_text[64];
    const char *day_labels[7] = {"S", "M", "T", "W", "T", "F", "S"};

    if(app == NULL)
        return;

    for(int i = 0; i < app->habits.count; i++) {
        if(habit_completed_today(&app->habits.items[i]))
            completed_count++;
    }

    locale_format(progress_text, sizeof(progress_text), "profile_habits_today_format",
                  completed_count, app->habits.count);
    if(stack_header) {
        profile_draw_fitted_text(progress_text, x, *y + flint_px(2), w, font,
                                 flint_theme_get_text());
        *y += flint_px(28);
        if(ui_draw_generic_button(x, *y, w, btn_h,
                                  locale_get("profile_manage_habits_button"),
                                  UI_BUTTON_STYLE_SECONDARY, 0, &manage_hover)) {
            app->profile_view = PROFILE_VIEW_HABITS;
            app->profile_scroll = 0;
            settings_screen_clear_status();
        }
    } else {
        int progress_w = w - btn_w - gap;

        profile_draw_fitted_text(progress_text, x, *y + flint_px(7), progress_w, font,
                                 flint_theme_get_text());
        if(ui_draw_generic_button(x + w - btn_w, *y, btn_w, btn_h,
                                  locale_get("profile_manage_habits_button"),
                                  UI_BUTTON_STYLE_SECONDARY, 0, &manage_hover)) {
            app->profile_view = PROFILE_VIEW_HABITS;
            app->profile_scroll = 0;
            settings_screen_clear_status();
        }
    }
    *y += btn_h + flint_px(8);

    if(app->habits.count <= 0) {
        flint_text_draw(locale_get("habit_empty_title"), x, *y + flint_px(6), small,
                        flint_darken(flint_theme_get_text(), 35));
        *y += cell + flint_px(6);
    } else {
        grid_y = *y;
        for(int day = 0; day < 7; day++) {
            int day_x = grid_x + day * (cell + cell_gap);
            int label_w_px = flint_text_measure(day_labels[day], small);

            flint_text_draw(day_labels[day], day_x + (cell - label_w_px) / 2,
                            grid_y, small, flint_darken(flint_theme_get_text(), 34));
        }
        *y += flint_px(20);
        for(int i = 0; i < app->habits.count; i++) {
            InbeHabit *habit = &app->habits.items[i];
            int row_y = *y;
            char short_name[8];
            int swatch = flint_px(8);
            const char *label = habit->name;
            char fitted_label[INBE_HABIT_NAME_SIZE + 4];
            int label_text_w = label_w - swatch - flint_px(7);

            if(label_w <= flint_px(60)) {
                inbe_text_short_label(habit->name, 3, 1, short_name, sizeof(short_name));
                label = short_name;
            } else if(flint_text_measure(label, small) > label_text_w) {
                inbe_text_fit_ellipsis(habit->name, fitted_label, sizeof(fitted_label),
                                       label_text_w, small);
                label = fitted_label;
            }
            DrawRectangle(overview_x, row_y + (cell - swatch) / 2, swatch, swatch,
                          habit->color);
            flint_text_draw(label, overview_x + swatch + flint_px(5),
                            flint_ui_text_y(label, row_y, cell, small),
                            small,
                                     flint_theme_get_text());
            for(int day = 0; day < 7; day++) {
                int day_index = profile_week_day_index(day);
                int completed = habit_completed_day(habit, day_index);
                int day_x = grid_x + day * (cell + cell_gap);
                Color fill = completed ? habit->color : flint_darken(flint_theme_get_bg(), 7);
                Color border = day_index == today_index
                                   ? flint_theme_get_text()
                                   : flint_darken(flint_theme_get_text(), 46);

                DrawRectangle(day_x, row_y, cell, cell, fill);
                DrawRectangleLines(day_x, row_y, cell, cell, border);
            }
            *y += cell + flint_px(6);
        }
    }

    profile_draw_divider(x, w, *y);
    *y += flint_px(18);
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

    profile_draw_habits_overview(app, x, w, y);

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
             profile_social_friends_count(app));
    snprintf(pending_text, sizeof(pending_text), locale_get("profile_pending_count_format"),
             profile_social_pending_count(app));
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
    int stack_rows = w < flint_px(260);

    *y += flint_px(16);

    for(int i = 0; i < app->habits.count; i++) {
        int row_y = *y;
        int up_hover = 0;
        int down_hover = 0;
        int controls_w = btn_w * 2 + gap;
        int name_w = stack_rows ? w : w - controls_w - gap;

        profile_draw_fitted_text(app->habits.items[i].name, x, row_y + flint_px(7),
                                 name_w, font, flint_theme_get_text());
        if(stack_rows)
            row_y += flint_px(36);
        if(ui_draw_generic_button(stack_rows ? x : x + w - btn_w * 2 - gap,
                                  row_y, btn_w, btn_h,
                                  locale_get("move_up_button"),
                                  UI_BUTTON_STYLE_SECONDARY, i == 0, &up_hover)) {
            if(habits_move(&app->habits, i, i - 1))
                app_auto_sync(app);
            return;
        }
        if(ui_draw_generic_button(stack_rows ? x + btn_w + gap : x + w - btn_w,
                                  row_y, btn_w, btn_h,
                                  locale_get("move_down_button"),
                                  UI_BUTTON_STYLE_SECONDARY,
                                  i == app->habits.count - 1, &down_hover)) {
            if(habits_move(&app->habits, i, i + 1))
                app_auto_sync(app);
            return;
        }
        *y += stack_rows ? flint_px(78) : flint_px(46);
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
        profile_social_draw_friends(app, page.content_x, page.content_w, &y);
    else if(app->profile_tab == PROFILE_TAB_LEADERBOARD)
        profile_social_draw_leaderboard(app, page.content_x, page.content_w, &y);
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
            profile_social_load_leaderboard_cache(app);
            app->profile_scroll = 0;
        }
        ui_set_dropdown_clip_top(0);
        ui_set_dropdown_clip_bottom(0);
    }
    return 0;
}
