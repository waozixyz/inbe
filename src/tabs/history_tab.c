#include "history_tab.h"
#include "app.h"
#include "data.h"
#include "locale.h"
#include "ui/ui.h"
#include "ui/text_layout.h"
#include "raylib.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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

/* Suppress GCC format-truncation warnings - paths are safely sized in practice */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"

/* Viewport dimensions - set by inbe_app_update_draw before calling draw functions */
extern int view_width;
extern int view_height;

/* Theme colors - set by ui_set_colors */
extern Color c_text, c_bg, c_circle, c_button, c_button_hover, c_icon;

/* ================================================================
 * INTERNAL HELPER FUNCTIONS
 * ================================================================ */

/* Check if a directory name consists only of digits */
static int
name_is_digits(const char *name, int len)
{
    int i;
    if(name == NULL)
        return 0;
    for(i = 0; i < len; i++) {
        if(name[i] == '\0' || name[i] < '0' || name[i] > '9')
            return 0;
    }
    return name[len] == '\0';
}

/* Compare two history entries for sorting (newest first) */
static int
compare_history_entries(const void *a, const void *b)
{
    const HistoryEntry *ea = a;
    const HistoryEntry *eb = b;
    return strcmp(eb->path, ea->path);
}

/* Scan a day directory for session files */
static void
scan_history_day(HistoryEntry *entries, int *count, int year, int month, int day, const char *path)
{
    DIR *dir = opendir(path);
    struct dirent *ent;
    char child[FS_PATH_MAX];

    if(dir == NULL)
        return;

    while((ent = readdir(dir)) != NULL && *count < HISTORY_MAX_SESSIONS) {
        if(ent->d_name[0] == '.')
            continue;
        snprintf(child, sizeof(child), "%s/%s", path, ent->d_name);
        if(strncmp(ent->d_name, "inbe-", 5) == 0) {
            /* Parse filename and add entry */
            const char *name = ent->d_name;
            int hh = 0, mm = 0, ss = 0;
            if(sscanf(name, "inbe-%2d%2d%2d", &hh, &mm, &ss) == 3) {
                HistoryEntry entry;
                memset(&entry, 0, sizeof(entry));
                snprintf(entry.path, sizeof(entry.path), "%s", child);
                entry.year = year;
                entry.month = month;
                entry.day = day;
                entry.hour = hh;
                entry.minute = mm;
                entry.second = ss;
                history_load_session_file(child, &entry);
                if(entry.round_count > 0) {
                    entries[*count] = entry;
                    (*count)++;
                }
            }
        }
    }
    closedir(dir);
}

/* Scan entire history tree and populate entries array */
static void
scan_history_tree(HistoryEntry *entries, int *count)
{
    DIR *years = opendir(data_root());
    struct dirent *year;
    char ypath[FS_PATH_MAX];
    char mpath[FS_PATH_MAX];
    char dpath[FS_PATH_MAX];

    *count = 0;
    if(years == NULL)
        return;

    while((year = readdir(years)) != NULL && *count < HISTORY_MAX_SESSIONS) {
        if(!name_is_digits(year->d_name, 4))
            continue;
        snprintf(ypath, sizeof(ypath), "%s/%s", data_root(), year->d_name);
        DIR *months = opendir(ypath);
        struct dirent *month;
        if(months == NULL)
            continue;
        while((month = readdir(months)) != NULL && *count < HISTORY_MAX_SESSIONS) {
            if(!name_is_digits(month->d_name, 2))
                continue;
            snprintf(mpath, sizeof(mpath), "%s/%s", ypath, month->d_name);
            DIR *days = opendir(mpath);
            struct dirent *day;
            if(days == NULL)
                continue;
            while((day = readdir(days)) != NULL && *count < HISTORY_MAX_SESSIONS) {
                if(!name_is_digits(day->d_name, 2))
                    continue;
                snprintf(dpath, sizeof(dpath), "%s/%s", mpath, day->d_name);
                scan_history_day(entries, count, atoi(year->d_name), atoi(month->d_name), atoi(day->d_name), dpath);
            }
            closedir(days);
        }
        closedir(months);
    }
    closedir(years);
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
        DrawRectangle(x, y, w, h, selected ? c_button_hover : ui_darken(c_button_hover, 6));
        ui_draw_bevel(x, y, w, h, ui_darken(c_button_hover, 40), ui_lighten(c_button_hover, 40));
        hover = 1;
        app->cursor_clickable = 1;
        if(IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            ui_draw_bevel(x, y, w, h, ui_lighten(c_button_hover, 40), ui_darken(c_button_hover, 40));
        }
    } else {
        DrawRectangle(x, y, w, h, selected ? c_button : ui_darken(c_bg, 6));
        ui_draw_bevel(x, y, w, h, ui_lighten(c_button, 28), ui_darken(c_button, 20));
    }

    DrawText(text, x + ui_px(indent), y + ui_px(6), ui_clamp_px(14, 12, 16), c_text);
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
    int font = ui_clamp_px(14, 12, 16);

    if(mx > x && mx < x + w && my > y && my < y + h) {
        DrawRectangle(x, y, w, h, selected ? c_button_hover : ui_darken(c_button_hover, 6));
        ui_draw_bevel(x, y, w, h, ui_darken(c_button_hover, 40), ui_lighten(c_button_hover, 40));
        hover = 1;
        app->cursor_clickable = 1;
        if(IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            ui_draw_bevel(x, y, w, h, ui_lighten(c_button_hover, 40), ui_darken(c_button_hover, 40));
        }
    } else {
        DrawRectangle(x, y, w, h, selected ? c_button : ui_darken(c_bg, 6));
        ui_draw_bevel(x, y, w, h, ui_lighten(c_button, 28), ui_darken(c_button, 20));
    }

    DrawText(text, x + ui_px(46), y + ui_px(6), font, c_text);

    if(hover && IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        return 1;
    return 0;
}

static int
draw_history_day_row(InbeApp *app, int x, int y, int w, int h, const char *text, int selected)
{
    int icon_size = ui_clamp_px(16, 14, 20);
    int icon_padding = ui_px(4);
    int icon_btn_w = icon_size + icon_padding * 2;
    int edit_x = x + w - icon_btn_w - ui_px(4);
    int edit_hover = 0;

    if(draw_history_row(app, x, y, w - icon_btn_w - ui_px(8), h, text, selected, 34))
        return 1;

    if(ui_draw_icon_btn_padded(app, edit_x, y + (h - icon_btn_w) / 2, icon_size, icon_padding,
                               app->pencil_icon, UI_ICON_TYPE_PENCIL, &edit_hover))
        return 2;

    return 0;
}

static int
draw_history_action_button(InbeApp *app, int right_x, int y, int row_h, int slot,
                           Texture2D icon, UIIconType icon_type)
{
    int icon_size = ui_clamp_px(16, 14, 20);
    int icon_padding = ui_px(4);
    int btn_w = icon_size + icon_padding * 2;
    int gap = ui_px(4);
    int hover = 0;
    int x = right_x - (slot + 1) * btn_w - (slot + 1) * gap;

    return ui_draw_icon_btn_padded(app, x, y + (row_h - btn_w) / 2,
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
        if(slash == NULL)
            return 0;
        *slash = '\0';

        snprintf(new_path, sizeof(new_path), "%s/inbe-%02d%02d%02d",
                 dir, hour, minute, entry->second);
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
        left_w = MeasureText(prefix, font);
        snprintf(prefix, sizeof(prefix), "%.*s", i + 1, text);
        right_w = MeasureText(prefix, font);

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
history_draw_edit_field(InbeApp *app, const HistoryEntry *entry, int x, int y, int w, int h)
{
    int font = ui_clamp_px(14, 12, 16);
    int valid = 0;
    int field_y = y + ui_px(3);
    int field_h = h - ui_px(6);
    int text_x = x + ui_px(8);
    int text_y = y + ui_px(7);
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

    DrawRectangle(x, field_y, w, field_h, ui_darken(c_bg, 10));
    ui_draw_bevel(x, field_y, w, field_h,
                  valid ? ui_lighten(c_button_hover, 35) : ui_lighten(c_button, 16),
                  valid ? ui_darken(c_button_hover, 30) : ui_darken(c_button, 34));
    DrawText(app->history_edit_text, text_x, text_y, font, c_text);
    if((app->inbe.frame / 24) % 2 == 0) {
        char prefix[16];
        snprintf(prefix, sizeof(prefix), "%.*s", app->history_edit_cursor,
                 app->history_edit_text);
        caret_x = text_x + MeasureText(prefix, font) + ui_px(1);
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
    FILE *file;
    int value;
    int total = 0;
    int count = 0;

    if(entry == NULL)
        return;

    entry->round_count = 0;
    entry->avg_seconds = 0;
    for(int i = 0; i < MaxRounds; i++)
        entry->rounds[i] = 0;

    file = fopen(path, "r");
    if(file == NULL)
        return;

    while(count < MaxRounds && fscanf(file, "%d", &value) == 1) {
        if(value <= 0)
            continue;
        entry->rounds[count] = value;
        total += value;
        count++;
    }
    fclose(file);

    entry->round_count = count;
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
    int font = ui_clamp_px(14, 12, 16);

    history_format_session_label(entry, out, out_size);
    if(view_width < 420 && MeasureText(out, font) > available_w)
        snprintf(out, out_size, "%02d:%02d  %ds", entry->hour, entry->minute, entry->avg_seconds);
}

void
history_tab_draw(InbeApp *app)
{
    HistoryEntry entries[HISTORY_MAX_SESSIONS];
    int count = 0;
    int title_h = ui_screen_header_height();
    int tab_h = ui_clamp_px(TAB_BAR_H, 54, 66);
    int viewport_h = view_height - title_h - tab_h;
    int row_h = ui_clamp_px(28, 24, 32);
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

    scan_history_tree(entries, &count);
    qsort(entries, (size_t)count, sizeof(entries[0]), compare_history_entries);

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
    content_h = ui_px(12);  /* Top padding */
    if(count > 0) {
        content_h += count * row_h;
    } else {
        content_h += viewport_h;
    }
    if(content_rows > 0)
        content_h = ui_px(12) + content_rows * row_h;
    content_h += ui_px(12);  /* Bottom padding */
    max_scroll = content_h - viewport_h;
    if(max_scroll < 0 || count == 0)
        max_scroll = 0;

    app->history_scroll -= (int)(GetMouseWheelMove() * 24.0f);
    app->history_scroll = (app->history_scroll < 0) ? 0 : (app->history_scroll > max_scroll ? max_scroll : app->history_scroll);

    /* Use percentage of screen width like tutorial, not DPI-scaled CONTENT_MAX_W */
    int responsive_max_w = (int)(view_width * 0.96f);
    int min_content_w = ui_px(320);
    if(responsive_max_w < min_content_w)
        responsive_max_w = min_content_w;
    int side_padding = ui_page_side_padding();
    ui_centered_column(responsive_max_w, side_padding, &content_x, &content_w);

    close_clicked = ui_draw_screen_header(app, locale_get("history_title"), 1);
    if(close_clicked) {
        app->inbe.screen = InbeScreenStart;
        app->history_scroll = 0;
        history_cancel_edit(app);
    }

    BeginScissorMode((int)app->camera.offset.x,
                     (int)(app->camera.offset.y + title_h * app->camera.zoom),
                     (int)(view_width * app->camera.zoom),
                     (int)(viewport_h * app->camera.zoom));
        y = title_h + ui_px(12) - app->history_scroll;
        if(count == 0) {
            int font = ui_clamp_px(14, 12, 16);
            const char *empty_text = locale_get("history_empty");
            TextLayout empty_layout = ui_text_layout_parse(empty_text, (Texture2D){0}, UI_ICON_TYPE_NONE, font);
            ui_text_layout_reflow(&empty_layout, content_w, font, ui_px(22));
            ui_text_layout_draw(&empty_layout, content_x, &y, font, c_text);
            ui_text_layout_free(&empty_layout);
        } else {
            if(app->history_level == HISTORY_LEVEL_EDIT_DAY && has_day) {
                int return_hover = 0;
                int icon_size = ui_clamp_px(16, 14, 20);
                int icon_padding = ui_px(4);
                if(ui_draw_icon_btn_padded(app, content_x, y + (row_h - icon_size - icon_padding * 2) / 2,
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
                    DrawText(label, content_x + icon_size + icon_padding * 2 + ui_px(10),
                             y + ui_px(6), ui_clamp_px(14, 12, 16), c_text);
                }
                y += row_h;

                for(int i = 0; i < count; i++) {
                    char time_label[HISTORY_TEXT_SIZE];
                    int right_edge = content_x + content_w;
                    int label_w = content_w - ui_px(76);
                    if(entries[i].year != app->history_year ||
                       entries[i].month != app->history_month ||
                       entries[i].day != app->history_day)
                        continue;

                    snprintf(time_label, sizeof(time_label), "%02d:%02d",
                             entries[i].hour, entries[i].minute);
                    DrawRectangle(content_x, y, content_w, row_h, ui_darken(c_bg, 6));
                    ui_draw_bevel(content_x, y, content_w, row_h, ui_lighten(c_button, 28), ui_darken(c_button, 20));
                    if(history_edit_matches(app, &entries[i], HISTORY_EDIT_TIME, -1)) {
                        history_draw_edit_field(app, &entries[i], content_x + ui_px(8), y,
                                                label_w, row_h);
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
                        DrawText(time_label, content_x + ui_px(10), y + ui_px(6),
                                 ui_clamp_px(14, 12, 16), c_text);
                        if(draw_history_action_button(app, right_edge, y, row_h, 1,
                                                      app->pencil_icon, UI_ICON_TYPE_PENCIL)) {
                            history_begin_edit_time(app, &entries[i]);
                        }
                    }
                    if(draw_history_action_button(app, right_edge, y, row_h, 0,
                                                  app->trash_icon, UI_ICON_TYPE_TRASH)) {
                        data_delete_session(entries[i].path);
                        history_clear_record_selection(app);
                        history_cancel_edit(app);
                        app->history_scroll = 0;
                        EndScissorMode();
                        return;
                    }
                    y += row_h;

                    for(int r = 0; r < entries[i].round_count; r++) {
                        char round_label[HISTORY_TEXT_SIZE];
                        history_format_round_label(&entries[i], r, round_label, sizeof(round_label));
                        DrawRectangle(content_x, y, content_w, row_h, ui_darken(c_bg, 4));
                        ui_draw_bevel(content_x, y, content_w, row_h, ui_lighten(c_button, 24), ui_darken(c_button, 18));
                        if(history_edit_matches(app, &entries[i], HISTORY_EDIT_ROUND, r)) {
                            history_draw_edit_field(app, &entries[i], content_x + ui_px(20), y,
                                                    label_w - ui_px(12), row_h);
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
                            DrawText(round_label, content_x + ui_px(22), y + ui_px(6),
                                     ui_clamp_px(14, 12, 16), c_text);
                            if(draw_history_action_button(app, right_edge, y, row_h, 1,
                                                          app->pencil_icon, UI_ICON_TYPE_PENCIL)) {
                                history_begin_edit_round(app, &entries[i], r);
                            }
                        }
                        if(draw_history_action_button(app, right_edge, y, row_h, 0,
                                                      app->trash_icon, UI_ICON_TYPE_TRASH)) {
                            delete_history_round(&entries[i], r);
                            history_clear_record_selection(app);
                            history_cancel_edit(app);
                            app->history_scroll = 0;
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
                    history_fit_session_label(&entries[i], content_w - ui_px(56), time_label, sizeof(time_label));
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
        int scrollbar_w = ui_clamp_px(6, 4, 8);
        int scrollbar_h = (viewport_h * viewport_h) / (content_h + viewport_h);
        int scrollbar_x = content_x + content_w + ui_px(10);
        int scrollbar_y = title_h + (app->history_scroll * (viewport_h - scrollbar_h)) / max_scroll;

        DrawRectangle(scrollbar_x, scrollbar_y, scrollbar_w, scrollbar_h, ui_darken(c_text, 40));
    }
}

#pragma GCC diagnostic pop
