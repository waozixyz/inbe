#include "history_tab.h"
#include "app.h"
#include "data.h"
#include "locale.h"
#include "flint_ui.h"
#include "raylib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define FS_PATH_MAX 512

enum {
    HISTORY_LEVEL_YEARS = 0,
    HISTORY_LEVEL_MONTHS = 1,
    HISTORY_LEVEL_DAYS = 2,
    HISTORY_LEVEL_SESSIONS = 3,
    HISTORY_LEVEL_EDIT_DAY = 4
};

enum {
    HISTORY_EDIT_NONE = 0,
    HISTORY_EDIT_TIME = 1,
    HISTORY_EDIT_ROUND = 2
};

enum {
    HISTORY_DELETE_NONE = 0,
    HISTORY_DELETE_SESSION = 1,
    HISTORY_DELETE_ROUND = 2
};

/* Suppress GCC format-truncation warnings - paths are safely sized in practice */
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#endif

/* Viewport dimensions - set by inbe_app_update_draw before calling draw functions */
extern int view_width;
extern int view_height;

/* Theme colors - set by ui_set_colors */
extern Color c_text, c_bg, c_circle, c_button, c_button_hover, c_icon;

/* ================================================================
 * INTERNAL HELPER FUNCTIONS
 * ================================================================ */

/* Compare two history entries for sorting (newest first) */
static int
compare_history_entries(const void *a, const void *b)
{
    const HistoryEntry *ea = a;
    const HistoryEntry *eb = b;
    if(ea->year != eb->year) return eb->year - ea->year;
    if(ea->month != eb->month) return eb->month - ea->month;
    if(ea->day != eb->day) return eb->day - ea->day;
    if(ea->hour != eb->hour) return eb->hour - ea->hour;
    if(ea->minute != eb->minute) return eb->minute - ea->minute;
    return eb->second - ea->second;
}

typedef struct ScanHistoryContext {
    HistoryEntry *entries;
    int *count;
} ScanHistoryContext;

static void
scan_history_callback(const char *path, int year, int month, int day,
                      int hour, int minute, int second,
                      int topic, int activity,
                      const int *round_times, int round_count, void *user)
{
    ScanHistoryContext *ctx = user;
    HistoryEntry entry;
    int total = 0;

    if(ctx == NULL || *ctx->count >= HISTORY_MAX_SESSIONS || round_times == NULL || round_count <= 0)
        return;
    (void)topic;
    (void)activity;
    memset(&entry, 0, sizeof(entry));
    snprintf(entry.path, sizeof(entry.path), "%s", path);
    entry.year = year;
    entry.month = month;
    entry.day = day;
    entry.hour = hour;
    entry.minute = minute;
    entry.second = second;
    entry.round_count = round_count > MaxRounds ? MaxRounds : round_count;
    for(int i = 0; i < entry.round_count; i++) {
        entry.rounds[i] = round_times[i];
        total += round_times[i];
        if(round_times[i] > entry.best)
            entry.best = round_times[i];
    }
    entry.avg_seconds = entry.round_count > 0 ? total / entry.round_count : 0;
    ctx->entries[*ctx->count] = entry;
    (*ctx->count)++;
}

/* Scan entire history tree and populate entries array */
static void
scan_history_tree(HistoryEntry *entries, int *count)
{
    ScanHistoryContext ctx;
    *count = 0;
    ctx.entries = entries;
    ctx.count = count;
    data_list_history(scan_history_callback, &ctx);
}

/* Check if history has entry for specific year */
static int
history_has_year(const HistoryEntry *entries, int count, int year)
{
    for(int i = 0; i < count; i++) {
        if(entries[i].year == year)
            return 1;
    }
    return 0;
}

/* Check if history has entry for specific month */
static int
history_has_month(const HistoryEntry *entries, int count, int year, int month)
{
    for(int i = 0; i < count; i++) {
        if(entries[i].year == year && entries[i].month == month)
            return 1;
    }
    return 0;
}

/* Check if history has entry for specific day */
static int
history_has_day_only(const HistoryEntry *entries, int count, int year, int month, int day)
{
    for(int i = 0; i < count; i++) {
        if(entries[i].year == year && entries[i].month == month && entries[i].day == day)
            return 1;
    }
    return 0;
}

/* Count year rows in history */
static int
history_count_year_rows(const HistoryEntry *entries, int count)
{
    int rows = 0;
    int last_year = 0;
    for(int i = 0; i < count; i++) {
        if(i == 0 || entries[i].year != last_year) {
            rows++;
            last_year = entries[i].year;
        }
    }
    return rows;
}

/* Count month rows for a specific year */
static int
history_count_month_rows(const HistoryEntry *entries, int count, int year)
{
    int rows = 0;
    int last_month = 0;
    int seen = 0;
    for(int i = 0; i < count; i++) {
        if(entries[i].year != year)
            continue;
        if(!seen || entries[i].month != last_month) {
            rows++;
            last_month = entries[i].month;
            seen = 1;
        }
    }
    return rows;
}

/* Count day rows for a specific month */
static int
history_count_day_rows(const HistoryEntry *entries, int count, int year, int month)
{
    int rows = 0;
    int last_day = 0;
    int seen = 0;
    for(int i = 0; i < count; i++) {
        if(entries[i].year != year || entries[i].month != month)
            continue;
        if(!seen || entries[i].day != last_day) {
            rows++;
            last_day = entries[i].day;
            seen = 1;
        }
    }
    return rows;
}

/* Count session records for a specific day */
static int
history_count_record_rows(const HistoryEntry *entries, int count, int year, int month, int day)
{
    int rows = 0;
    for(int i = 0; i < count; i++) {
        if(entries[i].year == year && entries[i].month == month && entries[i].day == day)
            rows++;
    }
    return rows;
}

/* Format round label (e.g., "R1  45s") */
static void
history_format_round_label(const HistoryEntry *entry, int round_index, char *out, int out_size)
{
    if(out == NULL || out_size <= 0)
        return;
    if(round_index >= 0 && round_index < entry->round_count)
        locale_format(out, (size_t)out_size, "history_round_label", round_index + 1, entry->rounds[round_index]);
    else
        locale_format(out, (size_t)out_size, "history_round_short_label", round_index + 1);
}

/* Draw a clickable history row */
static int
draw_history_row(InbeApp *app, int x, int y, int w, int h, const char *text, int selected, int indent)
{
    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
    int mx = (int)mouse_world.x;
    int my = (int)mouse_world.y;
    int hover = 0;

    if(mx > x && mx < x + w && my > y && my < y + h) {
        DrawRectangle(x, y, w, h, selected ? c_button_hover : flint_darken(c_button_hover, 6));
        ui_draw_bevel(x, y, w, h, flint_darken(c_button_hover, 40), flint_lighten(c_button_hover, 40));
        hover = 1;
        app->cursor_clickable = 1;
        if(IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            ui_draw_bevel(x, y, w, h, flint_lighten(c_button_hover, 40), flint_darken(c_button_hover, 40));
        }
    } else {
        DrawRectangle(x, y, w, h, selected ? c_button : flint_darken(c_bg, 6));
        ui_draw_bevel(x, y, w, h, flint_lighten(c_button, 28), flint_darken(c_button, 20));
    }

    flint_text_draw(text, x + flint_px(indent),
                    flint_ui_text_y(text, y, h, flint_px(16)),
                    flint_px(16), c_text);
    return hover && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

/* Draw a clickable history session row */
static int
draw_history_session_row(InbeApp *app, int x, int y, int w, int h, const char *text, int selected)
{
    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
    int mx = (int)mouse_world.x;
    int my = (int)mouse_world.y;
    int hover = 0;
    int font = flint_px(16);

    if(mx > x && mx < x + w && my > y && my < y + h) {
        DrawRectangle(x, y, w, h, selected ? c_button_hover : flint_darken(c_button_hover, 6));
        ui_draw_bevel(x, y, w, h, flint_darken(c_button_hover, 40), flint_lighten(c_button_hover, 40));
        hover = 1;
        app->cursor_clickable = 1;
        if(IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            ui_draw_bevel(x, y, w, h, flint_lighten(c_button_hover, 40), flint_darken(c_button_hover, 40));
        }
    } else {
        DrawRectangle(x, y, w, h, selected ? c_button : flint_darken(c_bg, 6));
        ui_draw_bevel(x, y, w, h, flint_lighten(c_button, 28), flint_darken(c_button, 20));
    }

    flint_text_draw(text, x + flint_px(46),
                    flint_ui_text_y(text, y, h, font), font, c_text);

    if(hover && IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        return 1;
    return 0;
}

static int
draw_history_day_row(InbeApp *app, int x, int y, int w, int h, const char *text, int selected)
{
    int icon_size = flint_px(16);
    int icon_padding = flint_px(4);
    int icon_btn_w = icon_size + icon_padding * 2;
    int edit_x = x + w - icon_btn_w - flint_px(4);
    int edit_hover = 0;

    if(draw_history_row(app, x, y, w - icon_btn_w - flint_px(8), h, text, selected, 34))
        return 1;

    if(ui_draw_icon_btn_padded(edit_x, y + (h - icon_btn_w) / 2, icon_size, icon_padding,
                               app->pencil_icon, UI_ICON_TYPE_PENCIL, &edit_hover))
        return 2;

    return 0;
}

static int
draw_history_action_button(InbeApp *app, int right_x, int y, int row_h, int slot,
                           Texture2D icon, UIIconType icon_type)
{
    (void)app;
    int icon_size = flint_px(16);
    int icon_padding = flint_px(4);
    int btn_w = icon_size + icon_padding * 2;
    int gap = flint_px(4);
    int hover = 0;
    int x = right_x - (slot + 1) * btn_w - (slot + 1) * gap;

    return ui_draw_icon_btn_padded(x, y + (row_h - btn_w) / 2,
                                   icon_size, icon_padding, icon, icon_type,
                                   &hover);
}

static void
history_cancel_edit(InbeApp *app)
{
    app->history_edit_active = 0;
    app->history_edit_kind = HISTORY_EDIT_NONE;
    app->history_edit_round = -1;
    app->history_edit_cursor = 0;
    app->history_edit_path[0] = '\0';
    app->history_edit_text[0] = '\0';
}

static void
history_cancel_pending_delete(InbeApp *app)
{
    app->history_delete_kind = HISTORY_DELETE_NONE;
    app->history_delete_round = -1;
    app->history_delete_path[0] = '\0';
    if(app->modal.type == UIModalConfirmDeleteHistory) {
        app->modal.active = 0;
        app->modal.type = UIModalNone;
        app->modal.selected_button = 0;
    }
}

static void
history_request_delete_session(InbeApp *app, const HistoryEntry *entry)
{
    if(entry == NULL)
        return;

    history_cancel_edit(app);
    app->history_delete_kind = HISTORY_DELETE_SESSION;
    app->history_delete_round = -1;
    snprintf(app->history_delete_path, sizeof(app->history_delete_path), "%s", entry->path);
    app->modal.active = 1;
    app->modal.type = UIModalConfirmDeleteHistory;
    app->modal.selected_button = 0;
}

static void
history_request_delete_round(InbeApp *app, const HistoryEntry *entry, int round_index)
{
    if(entry == NULL || round_index < 0 || round_index >= entry->round_count)
        return;

    history_cancel_edit(app);
    app->history_delete_kind = HISTORY_DELETE_ROUND;
    app->history_delete_round = round_index;
    snprintf(app->history_delete_path, sizeof(app->history_delete_path), "%s", entry->path);
    app->modal.active = 1;
    app->modal.type = UIModalConfirmDeleteHistory;
    app->modal.selected_button = 0;
}

static void
history_begin_edit_time(InbeApp *app, const HistoryEntry *entry)
{
    if(entry == NULL)
        return;

    app->history_edit_active = 1;
    app->history_edit_kind = HISTORY_EDIT_TIME;
    app->history_edit_round = -1;
    snprintf(app->history_edit_path, sizeof(app->history_edit_path), "%s", entry->path);
    snprintf(app->history_edit_text, sizeof(app->history_edit_text), "%02d:%02d",
             entry->hour, entry->minute);
    app->history_edit_cursor = (int)strlen(app->history_edit_text);
}

static void
history_begin_edit_round(InbeApp *app, const HistoryEntry *entry, int round_index)
{
    if(entry == NULL || round_index < 0 || round_index >= entry->round_count)
        return;

    app->history_edit_active = 1;
    app->history_edit_kind = HISTORY_EDIT_ROUND;
    app->history_edit_round = round_index;
    snprintf(app->history_edit_path, sizeof(app->history_edit_path), "%s", entry->path);
    snprintf(app->history_edit_text, sizeof(app->history_edit_text), "%d",
             entry->rounds[round_index]);
    app->history_edit_cursor = (int)strlen(app->history_edit_text);
}

static int
history_edit_matches(InbeApp *app, const HistoryEntry *entry, int kind, int round_index)
{
    return app->history_edit_active &&
           app->history_edit_kind == kind &&
           app->history_edit_round == round_index &&
           entry != NULL &&
           strcmp(app->history_edit_path, entry->path) == 0;
}

static int
history_parse_edit_time(const char *text, int *hour, int *minute)
{
    int h = -1;
    int m = -1;

    if(text == NULL)
        return 0;

    if(strlen(text) == 5 && text[2] == ':') {
        if(text[0] < '0' || text[0] > '9' ||
           text[1] < '0' || text[1] > '9' ||
           text[3] < '0' || text[3] > '9' ||
           text[4] < '0' || text[4] > '9')
            return 0;
        h = (text[0] - '0') * 10 + (text[1] - '0');
        m = (text[3] - '0') * 10 + (text[4] - '0');
    } else if(strlen(text) == 4) {
        for(int i = 0; i < 4; i++) {
            if(text[i] < '0' || text[i] > '9')
                return 0;
        }
        h = (text[0] - '0') * 10 + (text[1] - '0');
        m = (text[2] - '0') * 10 + (text[3] - '0');
    } else {
        return 0;
    }

    if(h < 0 || h > 23 || m < 0 || m > 59)
        return 0;

    *hour = h;
    *minute = m;
    return 1;
}

static int
history_parse_edit_seconds(const char *text, int *seconds)
{
    int value = 0;

    if(text == NULL || text[0] == '\0')
        return 0;

    for(int i = 0; text[i] != '\0'; i++) {
        if(text[i] < '0' || text[i] > '9')
            return 0;
        value = value * 10 + (text[i] - '0');
        if(value > 999)
            return 0;
    }

    if(value <= 0)
        return 0;

    *seconds = value;
    return 1;
}

static int
history_commit_edit(InbeApp *app, const HistoryEntry *entry)
{
    if(!app->history_edit_active || entry == NULL)
        return 0;

    if(app->history_edit_kind == HISTORY_EDIT_TIME) {
        int hour;
        int minute;
        char new_path[FS_PATH_MAX];
        char dir[FS_PATH_MAX];
        char *slash;

        if(!history_parse_edit_time(app->history_edit_text, &hour, &minute))
            return 0;

        snprintf(dir, sizeof(dir), "%s", entry->path);
        slash = strrchr(dir, '/');
        if(slash == NULL) {
            snprintf(new_path, sizeof(new_path), "inbe-%02d%02d%02d",
                     hour, minute, entry->second);
        } else {
            *slash = '\0';
            snprintf(new_path, sizeof(new_path), "%s/inbe-%02d%02d%02d",
                     dir, hour, minute, entry->second);
        }
        if(!data_rename_session(entry->path, new_path))
            return 0;

        history_cancel_edit(app);
        return 1;
    }

    if(app->history_edit_kind == HISTORY_EDIT_ROUND) {
        int seconds;
        int round_times[MaxRounds];

        if(app->history_edit_round < 0 || app->history_edit_round >= entry->round_count)
            return 0;
        if(!history_parse_edit_seconds(app->history_edit_text, &seconds))
            return 0;

        for(int i = 0; i < entry->round_count; i++)
            round_times[i] = entry->rounds[i];
        round_times[app->history_edit_round] = seconds;

        if(!data_replace_session(entry->path, round_times, entry->round_count))
            return 0;

        history_cancel_edit(app);
        return 1;
    }

    return 0;
}

static void
history_clamp_edit_cursor(InbeApp *app)
{
    int len = (int)strlen(app->history_edit_text);

    if(app->history_edit_cursor < 0)
        app->history_edit_cursor = 0;
    if(app->history_edit_cursor > len)
        app->history_edit_cursor = len;
}

static int
history_edit_cursor_from_x(const char *text, int font, int text_x, int target_x)
{
    int len;
    char prefix[16];

    if(text == NULL || target_x <= text_x)
        return 0;

    len = (int)strlen(text);
    for(int i = 0; i < len; i++) {
        int left_w;
        int right_w;

        snprintf(prefix, sizeof(prefix), "%.*s", i, text);
        left_w = flint_text_measure(prefix, font);
        snprintf(prefix, sizeof(prefix), "%.*s", i + 1, text);
        right_w = flint_text_measure(prefix, font);

        if(target_x < text_x + (left_w + right_w) / 2)
            return i;
    }

    return len;
}

static void
history_delete_before_cursor(InbeApp *app)
{
    size_t len = strlen(app->history_edit_text);
    int cursor = app->history_edit_cursor;

    if(cursor <= 0 || len == 0)
        return;

    memmove(app->history_edit_text + cursor - 1,
            app->history_edit_text + cursor,
            len - (size_t)cursor + 1);
    app->history_edit_cursor--;
}

static void
history_delete_at_cursor(InbeApp *app)
{
    size_t len = strlen(app->history_edit_text);
    int cursor = app->history_edit_cursor;

    if(cursor < 0 || cursor >= (int)len)
        return;

    memmove(app->history_edit_text + cursor,
            app->history_edit_text + cursor + 1,
            len - (size_t)cursor);
}

static void
history_insert_edit_char(InbeApp *app, char c)
{
    size_t len = strlen(app->history_edit_text);
    int max_len = (app->history_edit_kind == HISTORY_EDIT_TIME) ? 5 : 3;
    int cursor = app->history_edit_cursor;

    history_clamp_edit_cursor(app);
    cursor = app->history_edit_cursor;

    if(app->history_edit_kind == HISTORY_EDIT_TIME &&
       c >= '0' && c <= '9' &&
       cursor < (int)len &&
       app->history_edit_text[cursor] == ':') {
        cursor++;
        app->history_edit_cursor = cursor;
    }

    if(len < (size_t)max_len) {
        memmove(app->history_edit_text + cursor + 1,
                app->history_edit_text + cursor,
                len - (size_t)cursor + 1);
        app->history_edit_text[cursor] = c;
        app->history_edit_cursor = cursor + 1;
        return;
    }

    if(cursor < (int)len) {
        if(app->history_edit_kind == HISTORY_EDIT_TIME &&
           app->history_edit_text[cursor] == ':' &&
           c != ':')
            return;
        app->history_edit_text[cursor] = c;
        app->history_edit_cursor = cursor + 1;
    }
}

static int
history_should_show_keyboard(const InbeApp *app)
{
#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
    return app != NULL && app->history_edit_active;
#else
    return app != NULL && app->history_edit_active && app->on_screen_keyboard_enabled;
#endif
}

static int
history_keyboard_height(void)
{
    int key_h = flint_px(48);
    int gap = flint_px(6);
    int pad = flint_px(10);
    return pad * 2 + key_h * 4 + gap * 3;
}

static int
history_keyboard_key(InbeApp *app, int x, int y, int w, int h, const char *label)
{
    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
    Rectangle bounds = {(float)x, (float)y, (float)w, (float)h};
    int font = flint_px(16);
    int text_w;
    int pressed = 0;

    if(CheckCollisionPointRec(mouse_world, bounds)) {
        DrawRectangle(x, y, w, h, c_button_hover);
        ui_draw_bevel(x, y, w, h, flint_darken(c_button_hover, 40), flint_lighten(c_button_hover, 40));
        app->cursor_clickable = 1;
        if(IsMouseButtonDown(MOUSE_BUTTON_LEFT))
            ui_draw_bevel(x, y, w, h, flint_lighten(c_button_hover, 40), flint_darken(c_button_hover, 40));
        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
            pressed = 1;
    } else {
        DrawRectangle(x, y, w, h, c_button);
        ui_draw_bevel(x, y, w, h, flint_lighten(c_button, 40), flint_darken(c_button, 40));
    }

    text_w = flint_text_measure(label, font);
    flint_text_draw(label, x + (w - text_w) / 2,
                    flint_ui_text_y(label, y, h, font), font, c_text);
    return pressed;
}

static int
history_draw_keyboard(InbeApp *app, const HistoryEntry *entry)
{
    const char *labels[12] = {
        "1", "2", "3",
        "4", "5", "6",
        "7", "8", "9",
        "DEL", "0", "OK"
    };
    int keyboard_h = history_keyboard_height();
    int pad = flint_px(10);
    int gap = flint_px(6);
    int key_h = flint_px(48);
    int x = flint_page_side_padding();
    int y = view_height - keyboard_h;
    int w = view_width - x * 2;
    int key_w = (w - gap * 2) / 3;

    DrawRectangle(0, y, view_width, keyboard_h, flint_darken(c_bg, 10));
    DrawLine(0, y, view_width, y, flint_darken(c_bg, 42));

    for(int i = 0; i < 12; i++) {
        int col = i % 3;
        int row = i / 3;
        int key_x = x + col * (key_w + gap);
        int key_y = y + pad + row * (key_h + gap);

        if(history_keyboard_key(app, key_x, key_y, key_w, key_h, labels[i])) {
            if(i == 9) {
                history_delete_before_cursor(app);
            } else if(i == 11) {
                if(history_commit_edit(app, entry))
                    return 1;
            } else {
                history_insert_edit_char(app, labels[i][0]);
            }
        }
    }

    return 0;
}

static void
history_update_edit_input(InbeApp *app, const HistoryEntry *entry,
                          int field_x, int field_y, int field_w, int field_h,
                          int text_x, int font)
{
    int ch;

    if(!app->history_edit_active)
        return;

    history_clamp_edit_cursor(app);

    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
        int mx = (int)mouse_world.x;
        int my = (int)mouse_world.y;

        if(mx >= field_x && mx <= field_x + field_w &&
           my >= field_y && my <= field_y + field_h) {
            app->history_edit_cursor = history_edit_cursor_from_x(app->history_edit_text,
                                                                  font, text_x, mx);
        }
    }

    if(IsKeyPressed(KEY_ESCAPE)) {
        history_cancel_edit(app);
        return;
    }

    if(IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        history_commit_edit(app, entry);
        return;
    }

    if(IsKeyPressed(KEY_LEFT))
        app->history_edit_cursor--;
    if(IsKeyPressed(KEY_RIGHT))
        app->history_edit_cursor++;
    if(IsKeyPressed(KEY_HOME))
        app->history_edit_cursor = 0;
    if(IsKeyPressed(KEY_END))
        app->history_edit_cursor = (int)strlen(app->history_edit_text);
    history_clamp_edit_cursor(app);

    if(IsKeyPressed(KEY_BACKSPACE)) {
        history_delete_before_cursor(app);
    }
    if(IsKeyPressed(KEY_DELETE)) {
        history_delete_at_cursor(app);
    }

    ch = GetCharPressed();
    while(ch > 0) {
        int allowed = 0;

        if(ch >= '0' && ch <= '9')
            allowed = 1;
        if(app->history_edit_kind == HISTORY_EDIT_TIME && ch == ':')
            allowed = 1;

        if(allowed)
            history_insert_edit_char(app, (char)ch);
        ch = GetCharPressed();
    }

    history_clamp_edit_cursor(app);
}

static void
history_draw_edit_field(InbeApp *app, const HistoryEntry *entry, int x, int y, int w, int h, int font)
{
    int valid = 0;
    int field_y = y + flint_px(3);
    int field_h = h - flint_px(6);
    int text_x = x + flint_px(8);
    int text_y = flint_ui_text_y(app->history_edit_text, y, h, font);
    int caret_x;

    history_update_edit_input(app, entry, x, field_y, w, field_h, text_x, font);

    if(!app->history_edit_active)
        return;

    if(app->history_edit_kind == HISTORY_EDIT_TIME) {
        int hour;
        int minute;
        valid = history_parse_edit_time(app->history_edit_text, &hour, &minute);
    } else if(app->history_edit_kind == HISTORY_EDIT_ROUND) {
        int seconds;
        valid = history_parse_edit_seconds(app->history_edit_text, &seconds);
    }

    history_clamp_edit_cursor(app);

    DrawRectangle(x, field_y, w, field_h, flint_darken(c_bg, 10));
    ui_draw_bevel(x, field_y, w, field_h,
                  valid ? flint_lighten(c_button_hover, 35) : flint_lighten(c_button, 16),
                  valid ? flint_darken(c_button_hover, 30) : flint_darken(c_button, 34));
    flint_text_draw(app->history_edit_text, text_x, text_y, font, c_text);
    if((app->inbe.frame / 24) % 2 == 0) {
        char prefix[16];
        snprintf(prefix, sizeof(prefix), "%.*s", app->history_edit_cursor,
                 app->history_edit_text);
        caret_x = text_x + flint_text_measure(prefix, font) + flint_px(1);
        DrawLine(caret_x, text_y, caret_x, text_y + font, c_text);
    }
}

static int
delete_history_round(const HistoryEntry *entry, int round_index)
{
    int round_times[MaxRounds];
    int count = 0;

    if(entry == NULL || round_index < 0 || round_index >= entry->round_count)
        return 0;

    for(int i = 0; i < entry->round_count; i++) {
        if(i == round_index)
            continue;
        round_times[count++] = entry->rounds[i];
    }

    return data_replace_session(entry->path, round_times, count);
}

static int
history_execute_pending_delete(InbeApp *app)
{
    if(app->history_delete_kind == HISTORY_DELETE_SESSION) {
        if(data_delete_session(app->history_delete_path)) {
            history_clear_record_selection(app);
            history_cancel_pending_delete(app);
            app->history_scroll = 0;
            return 1;
        }
    } else if(app->history_delete_kind == HISTORY_DELETE_ROUND) {
        HistoryEntry entry;
        memset(&entry, 0, sizeof(entry));
        snprintf(entry.path, sizeof(entry.path), "%s", app->history_delete_path);
        history_load_session_file(entry.path, &entry);
        if(delete_history_round(&entry, app->history_delete_round)) {
            history_clear_record_selection(app);
            history_cancel_pending_delete(app);
            app->history_scroll = 0;
            return 1;
        }
    }

    return 0;
}

/* ================================================================
 * PUBLIC API
 * ================================================================ */

void
history_tab_on_click(void *user_data)
{
    InbeApp *app = user_data;
    history_open_latest(app);
    app->inbe.screen = InbeScreenHistory;
}

void
history_tab_reset(InbeApp *app)
{
    app->history_scroll = 0;
    app->history_drag_scrollbar = 0;
    app->history_drag_content = 0;
    app->history_drag_content_y = 0;
    app->history_level = 0;
    app->history_year = 0;
    app->history_month = 0;
    app->history_day = 0;
    app->history_record[0] = 0;
    history_cancel_edit(app);
    history_cancel_pending_delete(app);
}

int
history_tab_handle_back(InbeApp *app)
{
    if(app->history_edit_active) {
        history_cancel_edit(app);
        return 1;
    }

    if(app->history_level >= HISTORY_LEVEL_EDIT_DAY) {
        app->history_level = HISTORY_LEVEL_SESSIONS;
        app->history_scroll = 0;
        return 1;
    }

    return 0;
}

int
history_tab_is_editing(const InbeApp *app)
{
    return app != NULL && app->history_edit_active;
}

void
history_open_latest(InbeApp *app)
{
    HistoryEntry entries[HISTORY_MAX_SESSIONS];
    int count = 0;
    time_t now;
    struct tm *tm;

    scan_history_tree(entries, &count);
    qsort(entries, (size_t)count, sizeof(entries[0]), compare_history_entries);

    if(count > 0) {
        now = time(NULL);
        tm = localtime(&now);
        if(tm != NULL) {
            for(int i = 0; i < count; i++) {
                if(entries[i].year == tm->tm_year + 1900 &&
                   entries[i].month == tm->tm_mon + 1 &&
                   entries[i].day == tm->tm_mday) {
                    app->history_year = entries[i].year;
                    app->history_month = entries[i].month;
                    app->history_day = entries[i].day;
                    app->history_level = 2;
                    app->history_record[0] = 0;
                    app->history_scroll = 0;
                    return;
                }
            }
        }

        app->history_year = entries[0].year;
        app->history_month = entries[0].month;
        app->history_day = entries[0].day;
        app->history_level = 2;
        app->history_record[0] = 0;
        app->history_scroll = 0;
        return;
    }

    now = time(NULL);
    tm = localtime(&now);
    if(tm != NULL) {
        app->history_year = tm->tm_year + 1900;
        app->history_month = tm->tm_mon + 1;
        app->history_day = tm->tm_mday;
    } else {
        app->history_year = 0;
        app->history_month = 0;
        app->history_day = 0;
    }
    app->history_level = 0;
    history_clear_record_selection(app);
    app->history_scroll = 0;
}

void
history_load_session_file(const char *path, HistoryEntry *entry)
{
    int total = 0;
    int count;
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;

    if(entry == NULL)
        return;

    entry->round_count = 0;
    entry->avg_seconds = 0;
    for(int i = 0; i < MaxRounds; i++)
        entry->rounds[i] = 0;

    count = data_load_session(path, entry->rounds, MaxRounds,
                              &year, &month, &day, &hour, &minute, &second);
    if(count <= 0)
        return;

    snprintf(entry->path, sizeof(entry->path), "%s", path);
    entry->year = year;
    entry->month = month;
    entry->day = day;
    entry->hour = hour;
    entry->minute = minute;
    entry->second = second;
    entry->round_count = count;
    for(int i = 0; i < count; i++) {
        total += entry->rounds[i];
        if(entry->rounds[i] > entry->best)
            entry->best = entry->rounds[i];
    }
    if(count > 0)
        entry->avg_seconds = total / count;
}

void
history_clear_record_selection(InbeApp *app)
{
    app->history_record[0] = 0;
}

void
history_format_session_label(const HistoryEntry *entry, char *out, size_t out_size)
{
    if(out == NULL || out_size == 0)
        return;
    locale_format(out, out_size, "history_session_label", entry->hour, entry->minute, entry->avg_seconds);
}

static void
history_fit_session_label(const HistoryEntry *entry, int available_w, char *out, size_t out_size)
{
    int font = flint_px(16);

    history_format_session_label(entry, out, out_size);
    if(view_width < 420 && flint_text_measure(out, font) > available_w)
        snprintf(out, out_size, "%02d:%02d  %ds", entry->hour, entry->minute, entry->avg_seconds);
}

static int
history_active_edit_row(const InbeApp *app, const HistoryEntry *entries, int count)
{
    int row = 1;

    if(app == NULL || entries == NULL || !app->history_edit_active ||
       app->history_level != HISTORY_LEVEL_EDIT_DAY)
        return -1;

    for(int i = 0; i < count; i++) {
        if(entries[i].year != app->history_year ||
           entries[i].month != app->history_month ||
           entries[i].day != app->history_day)
            continue;

        if(strcmp(app->history_edit_path, entries[i].path) == 0) {
            if(app->history_edit_kind == HISTORY_EDIT_TIME)
                return row;
            if(app->history_edit_kind == HISTORY_EDIT_ROUND &&
               app->history_edit_round >= 0 &&
               app->history_edit_round < entries[i].round_count)
                return row + 1 + app->history_edit_round;
        }

        row += 1 + entries[i].round_count;
    }

    return -1;
}

static void
history_scroll_edit_into_view(InbeApp *app, const HistoryEntry *entries, int count,
                              int row_h, int viewport_h, int max_scroll)
{
    static char last_path[FS_PATH_MAX] = "";
    static int last_kind = HISTORY_EDIT_NONE;
    static int last_round = -1;
    int row = history_active_edit_row(app, entries, count);
    int margin = flint_px(10);
    int row_top;
    int row_bottom;
    int changed;

    if(app == NULL || !app->history_edit_active) {
        last_path[0] = '\0';
        last_kind = HISTORY_EDIT_NONE;
        last_round = -1;
        return;
    }

    if(row < 0 || max_scroll <= 0 || viewport_h <= 0)
        return;

    changed = last_kind != app->history_edit_kind ||
              last_round != app->history_edit_round ||
              strcmp(last_path, app->history_edit_path) != 0;
    if(!changed)
        return;

    snprintf(last_path, sizeof(last_path), "%s", app->history_edit_path);
    last_kind = app->history_edit_kind;
    last_round = app->history_edit_round;

    row_top = flint_px(12) + row * row_h;
    row_bottom = row_top + row_h;

    if(row_top - margin < app->history_scroll)
        app->history_scroll = row_top - margin;
    else if(row_bottom + margin > app->history_scroll + viewport_h)
        app->history_scroll = row_bottom + margin - viewport_h;

    if(app->history_scroll < 0)
        app->history_scroll = 0;
    if(app->history_scroll > max_scroll)
        app->history_scroll = max_scroll;
}

void
history_tab_draw(InbeApp *app)
{
    HistoryEntry entries[HISTORY_MAX_SESSIONS];
    HistoryEntry *keyboard_entry = NULL;
    int count = 0;
    int title_h = ui_screen_header_height();
    int tab_h = flint_px(56);
    int keyboard_h = history_should_show_keyboard(app) ? history_keyboard_height() : 0;
    int viewport_h = view_height - title_h - tab_h - keyboard_h;
    int row_h = flint_px(32);
    int content_rows = 0;
    int content_h = 0;
    int max_scroll;
    int close_clicked = 0;
    int y;
    int has_year = 0;
    int has_month = 0;
    int has_day = 0;
    int selected_index = -1;
    int content_x;
    int content_w;
    int scrollbar_w = flint_px(8);
    int scrollbar_gap = flint_px(6);

    scan_history_tree(entries, &count);
    qsort(entries, (size_t)count, sizeof(entries[0]), compare_history_entries);

    if(app->history_edit_active) {
        for(int i = 0; i < count; i++) {
            if(strcmp(app->history_edit_path, entries[i].path) == 0) {
                keyboard_entry = &entries[i];
                break;
            }
        }
    }

    if(count > 0) {
        if(app->history_level <= 0 || app->history_year == 0) {
            history_open_latest(app);
        } else if(app->history_level == 1 && !history_has_year(entries, count, app->history_year)) {
            history_open_latest(app);
        } else if(app->history_level == 2 && !history_has_month(entries, count, app->history_year, app->history_month)) {
            history_open_latest(app);
        } else if(app->history_level >= 3 &&
                  !history_has_day_only(entries, count, app->history_year, app->history_month, app->history_day)) {
            history_open_latest(app);
        }
    }

    has_year = history_has_year(entries, count, app->history_year);
    has_month = history_has_month(entries, count, app->history_year, app->history_month);
    has_day = history_has_day_only(entries, count, app->history_year, app->history_month, app->history_day);

    if(count > 0 && has_day && app->history_level >= HISTORY_LEVEL_SESSIONS) {
        for(int i = 0; i < count; i++) {
            char record_name[16];
            if(entries[i].year != app->history_year ||
               entries[i].month != app->history_month ||
               entries[i].day != app->history_day)
                continue;
            snprintf(record_name, sizeof(record_name), "inbe-%02d%02d%02d",
                     entries[i].hour, entries[i].minute, entries[i].second);
            if(strcmp(app->history_record, record_name) == 0) {
                selected_index = i;
                break;
            }
        }
    }

    content_rows = history_count_year_rows(entries, count);
    if(count > 0 && has_year && app->history_level >= HISTORY_LEVEL_MONTHS) {
        content_rows += history_count_month_rows(entries, count, app->history_year);
        if(has_month && app->history_level >= HISTORY_LEVEL_DAYS) {
            content_rows += history_count_day_rows(entries, count, app->history_year, app->history_month);
            if(has_day && app->history_level >= HISTORY_LEVEL_SESSIONS) {
                if(app->history_level == HISTORY_LEVEL_EDIT_DAY) {
                    content_rows = 1;
                    for(int i = 0; i < count; i++) {
                        if(entries[i].year == app->history_year &&
                           entries[i].month == app->history_month &&
                           entries[i].day == app->history_day)
                            content_rows += 1 + entries[i].round_count;
                    }
                } else {
                    content_rows += history_count_record_rows(entries, count,
                                                             app->history_year, app->history_month,
                                                             app->history_day);
                    if(selected_index >= 0)
                        content_rows += entries[selected_index].round_count;
                }
            }
        }
    }

    /* Calculate content height with proper top and bottom padding */
    content_h = flint_px(12);  /* Top padding */
    if(count > 0) {
        content_h += count * row_h;
    } else {
        content_h += viewport_h;
    }
    if(content_rows > 0)
        content_h = flint_px(12) + content_rows * row_h;
    content_h += flint_px(12);  /* Bottom padding */
    max_scroll = content_h - viewport_h;
    if(max_scroll < 0 || count == 0)
        max_scroll = 0;

    app->history_scroll -= (int)(GetMouseWheelMove() * 24.0f);
    app->history_scroll = (app->history_scroll < 0) ? 0 : (app->history_scroll > max_scroll ? max_scroll : app->history_scroll);
    history_scroll_edit_into_view(app, entries, count, row_h, viewport_h, max_scroll);

    /* Use percentage of screen width like tutorial, not DPI-scaled CONTENT_MAX_W */
    int responsive_max_w = (int)(view_width * 0.96f);
    int min_content_w = flint_px(320);
    if(responsive_max_w < min_content_w)
        responsive_max_w = min_content_w;
    int side_padding = flint_page_side_padding();
    flint_centered_column(responsive_max_w, side_padding, &content_x, &content_w);
    if(max_scroll > 0 && content_w > flint_px(120) + scrollbar_w + scrollbar_gap)
        content_w -= scrollbar_w + scrollbar_gap;

    close_clicked = ui_draw_screen_header(locale_get("history_title"), 1);
    if(close_clicked) {
        app->inbe.screen = InbeScreenStart;
        app->history_scroll = 0;
        history_cancel_edit(app);
    }

    BeginScissorMode((int)app->camera.offset.x,
                     (int)(app->camera.offset.y + title_h * app->camera.zoom),
                     (int)(view_width * app->camera.zoom),
                     (int)(viewport_h * app->camera.zoom));
        y = title_h + flint_px(12) - app->history_scroll;
        if(count == 0) {
            int font = flint_px(16);
            const char *empty_text = locale_get("history_empty");
            flint_ui_paragraph_draw((FlintUIParagraph){
                .text = empty_text,
                .width = content_w,
                .font = font,
                .line_gap = flint_px(22),
                .color = c_text,
            }, content_x, &y);
        } else {
            if(app->history_level == HISTORY_LEVEL_EDIT_DAY && has_day) {
                int return_hover = 0;
                int icon_size = flint_px(16);
                int icon_padding = flint_px(4);
                if(ui_draw_icon_btn_padded(content_x, y + (row_h - icon_size - icon_padding * 2) / 2,
                                           icon_size, icon_padding, app->return_icon,
                                           UI_ICON_TYPE_RETURN, &return_hover)) {
                    app->history_level = HISTORY_LEVEL_SESSIONS;
                    app->history_scroll = 0;
                    history_cancel_edit(app);
                    EndScissorMode();
                    return;
                }
                {
                    char label[HISTORY_TEXT_SIZE];
                    locale_format(label, sizeof(label), "history_day_label", app->history_day);
                    flint_text_draw(label, content_x + icon_size + icon_padding * 2 + flint_px(10),
                                    flint_ui_text_y(label, y, row_h, flint_px(16)),
                                    flint_px(16), c_text);
                }
                y += row_h;

                for(int i = 0; i < count; i++) {
                    char time_label[HISTORY_TEXT_SIZE];
                    int right_edge = content_x + content_w;
                    int label_w = content_w - flint_px(76);
                    if(entries[i].year != app->history_year ||
                       entries[i].month != app->history_month ||
                       entries[i].day != app->history_day)
                        continue;

                    snprintf(time_label, sizeof(time_label), "%02d:%02d",
                             entries[i].hour, entries[i].minute);
                    DrawRectangle(content_x, y, content_w, row_h, flint_darken(c_bg, 6));
                    ui_draw_bevel(content_x, y, content_w, row_h, flint_lighten(c_button, 28), flint_darken(c_button, 20));
                    if(history_edit_matches(app, &entries[i], HISTORY_EDIT_TIME, -1)) {
                        history_draw_edit_field(app, &entries[i], content_x + flint_px(2), y,
                                                label_w, row_h, flint_px(16));
                        if(!app->history_edit_active) {
                            app->history_scroll = 0;
                            EndScissorMode();
                            return;
                        }
                        if(draw_history_action_button(app, right_edge, y, row_h, 1,
                                                      app->save_icon, UI_ICON_TYPE_SAVE)) {
                            if(history_commit_edit(app, &entries[i])) {
                                app->history_scroll = 0;
                                EndScissorMode();
                                return;
                            }
                        }
                    } else {
                        flint_text_draw(time_label, content_x + flint_px(10),
                                        flint_ui_text_y(time_label, y, row_h, flint_px(16)),
                                        flint_px(16), c_text);
                        if(draw_history_action_button(app, right_edge, y, row_h, 1,
                                                      app->pencil_icon, UI_ICON_TYPE_PENCIL)) {
                            history_begin_edit_time(app, &entries[i]);
                        }
                    }
                    if(draw_history_action_button(app, right_edge, y, row_h, 0,
                                                  app->trash_icon, UI_ICON_TYPE_TRASH)) {
                        history_request_delete_session(app, &entries[i]);
                        EndScissorMode();
                        return;
                    }
                    y += row_h;

                    for(int r = 0; r < entries[i].round_count; r++) {
                        char round_label[HISTORY_TEXT_SIZE];
                        history_format_round_label(&entries[i], r, round_label, sizeof(round_label));
                        DrawRectangle(content_x, y, content_w, row_h, flint_darken(c_bg, 4));
                        ui_draw_bevel(content_x, y, content_w, row_h, flint_lighten(c_button, 24), flint_darken(c_button, 18));
                        if(history_edit_matches(app, &entries[i], HISTORY_EDIT_ROUND, r)) {
                            history_draw_edit_field(app, &entries[i], content_x + flint_px(14), y,
                                                    label_w - flint_px(12), row_h, flint_px(16));
                            if(!app->history_edit_active) {
                                app->history_scroll = 0;
                                EndScissorMode();
                                return;
                            }
                            if(draw_history_action_button(app, right_edge, y, row_h, 1,
                                                          app->save_icon, UI_ICON_TYPE_SAVE)) {
                                if(history_commit_edit(app, &entries[i])) {
                                    app->history_scroll = 0;
                                    EndScissorMode();
                                    return;
                                }
                            }
                        } else {
                            flint_text_draw(round_label, content_x + flint_px(22),
                                            flint_ui_text_y(round_label, y, row_h, flint_px(16)),
                                            flint_px(16), c_text);
                            if(draw_history_action_button(app, right_edge, y, row_h, 1,
                                                          app->pencil_icon, UI_ICON_TYPE_PENCIL)) {
                                history_begin_edit_round(app, &entries[i], r);
                            }
                        }
                        if(draw_history_action_button(app, right_edge, y, row_h, 0,
                                                      app->trash_icon, UI_ICON_TYPE_TRASH)) {
                            history_request_delete_round(app, &entries[i], r);
                            EndScissorMode();
                            return;
                        }
                        y += row_h;
                    }
                }
            } else {
                int year = -1;
                int month = -1;
                int day = -1;

                for(int i = 0; i < count; i++) {
                char label[HISTORY_TEXT_SIZE];

                if(entries[i].year != year) {
                    int selected = app->history_year == entries[i].year && app->history_level >= HISTORY_LEVEL_MONTHS;
                    snprintf(label, sizeof(label), "%04d", entries[i].year);
                    if(draw_history_row(app, content_x, y, content_w, row_h, label, selected, 10)) {
                        app->history_year = entries[i].year;
                        app->history_month = 0;
                        app->history_day = 0;
                        history_clear_record_selection(app);
                        app->history_level = 1;
                        app->history_scroll = 0;
                    }
                    y += row_h;
                    year = entries[i].year;
                    month = -1;
                    day = -1;
                }

                if(app->history_level < HISTORY_LEVEL_MONTHS || entries[i].year != app->history_year)
                    continue;

                if(entries[i].month != month) {
                    int selected = app->history_month == entries[i].month && app->history_level >= HISTORY_LEVEL_DAYS;
                    locale_format(label, sizeof(label), "history_month_label", entries[i].month);
                    if(draw_history_row(app, content_x, y, content_w, row_h, label, selected, 22)) {
                        app->history_month = entries[i].month;
                        app->history_day = 0;
                        history_clear_record_selection(app);
                        app->history_level = 2;
                        app->history_scroll = 0;
                    }
                    y += row_h;
                    month = entries[i].month;
                    day = -1;
                }

                if(app->history_level < HISTORY_LEVEL_DAYS || entries[i].month != app->history_month)
                    continue;

                if(entries[i].day != day) {
                    int selected = app->history_day == entries[i].day && app->history_level >= HISTORY_LEVEL_SESSIONS;
                    int result;
                    locale_format(label, sizeof(label), "history_day_label", entries[i].day);
                    result = draw_history_day_row(app, content_x, y, content_w, row_h, label, selected);
                    if(result == 1) {
                        app->history_day = entries[i].day;
                        history_clear_record_selection(app);
                        app->history_level = HISTORY_LEVEL_SESSIONS;
                        app->history_scroll = 0;
                    } else if(result == 2) {
                        app->history_day = entries[i].day;
                        history_clear_record_selection(app);
                        app->history_level = HISTORY_LEVEL_EDIT_DAY;
                        app->history_scroll = 0;
                    }
                    y += row_h;
                    day = entries[i].day;
                }

                if(app->history_level < HISTORY_LEVEL_SESSIONS || entries[i].day != app->history_day)
                    continue;

                {
                    char time_label[HISTORY_TEXT_SIZE];
                    char record_name[16];
                    int selected;
                    int result;

                    snprintf(record_name, sizeof(record_name), "inbe-%02d%02d%02d",
                             entries[i].hour, entries[i].minute, entries[i].second);
                    history_fit_session_label(&entries[i], content_w - flint_px(56), time_label, sizeof(time_label));
                    selected = strcmp(app->history_record, record_name) == 0;
                    result = draw_history_session_row(app, content_x, y, content_w, row_h, time_label, selected);

                    if(result == 1) {
                        snprintf(app->history_record, sizeof(app->history_record), "inbe-%02d%02d%02d",
                                 entries[i].hour, entries[i].minute, entries[i].second);
                        app->history_level = 3;
                        selected_index = i;
                    }
                    y += row_h;

                    if(selected_index == i) {
                        for(int r = 0; r < entries[i].round_count; r++) {
                            char round_label[HISTORY_TEXT_SIZE];
                            history_format_round_label(&entries[i], r, round_label, sizeof(round_label));
                            if(draw_history_row(app, content_x, y, content_w, row_h, round_label, 0, 10)) {
                                /* Round clicked - could do something here */
                            }
                            y += row_h;
                        }
                    }
                }
            }
            }
        }
    EndScissorMode();

    /* Draw scrollbar if needed */
    if(max_scroll > 0) {
        int scrollbar_x = (int)(app->camera.offset.x + (content_x + content_w + scrollbar_gap) * app->camera.zoom);
        int scrollbar_y = (int)(app->camera.offset.y + title_h * app->camera.zoom);
        int scrollbar_viewport_h = (int)(viewport_h * app->camera.zoom);
        int scrollbar_content_h = (int)(content_h * app->camera.zoom);

        ui_draw_scrollbar(scrollbar_x, scrollbar_y, scrollbar_viewport_h,
                          scrollbar_content_h, &app->history_scroll, max_scroll);
    }

    if(history_should_show_keyboard(app) && keyboard_entry != NULL) {
        if(history_draw_keyboard(app, keyboard_entry))
            app->history_scroll = 0;
    }

    if(app->modal.active && app->modal.type == UIModalConfirmDeleteHistory) {
        int modal_result = ui_draw_modal(locale_get("delete_history_title"),
                                         app->history_delete_kind == HISTORY_DELETE_ROUND
                                             ? locale_get("delete_round_message")
                                             : locale_get("delete_session_message"),
                                         locale_get("cancel_button"),
                                         locale_get("delete_button"));
        if(modal_result == 1) {
            history_cancel_pending_delete(app);
        } else if(modal_result == 2) {
            history_execute_pending_delete(app);
        }
    } else if(app->history_delete_kind != HISTORY_DELETE_NONE) {
        history_cancel_pending_delete(app);
    }
}

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
