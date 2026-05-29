#include "history_tab.h"
#include "app.h"
#include "data.h"
#include "ui.h"
#include "text_layout.h"
#include "raylib.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define FS_PATH_MAX 512

/* Suppress GCC format-truncation warnings - paths are safely sized in practice */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"

#define CONTENT_MAX_W 440
#define CONTENT_SIDE_PAD 16
#define SETTINGS_TITLE_H 60
#define TAB_BAR_H 54
#define ICON_SIZE_SMALL 16
#define ICON_SIZE_SMALL_MIN 14
#define ICON_SIZE_SMALL_MAX 18

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
        snprintf(out, (size_t)out_size, "R%d  %ds", round_index + 1, entry->rounds[round_index]);
    else
        snprintf(out, (size_t)out_size, "R%d", round_index + 1);
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

/* Draw a history session row with trash icon */
static int
draw_history_session_row(InbeApp *app, int x, int y, int w, int h, const char *text, int selected)
{
    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
    int mx = (int)mouse_world.x;
    int my = (int)mouse_world.y;
    int hover = 0;
    int icon_size = ui_clamp_px(ICON_SIZE_SMALL, ICON_SIZE_SMALL_MIN, ICON_SIZE_SMALL_MAX);
    int font = ui_clamp_px(14, 12, 16);
    (void)font;

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

    if(app->x_icon.id != 0) {
        int trash_x = x + w - icon_size - ui_px(8);
        int trash_y = y + (h - icon_size) / 2;
        Rectangle src = {0, 0, app->x_icon.width, app->x_icon.height};
        Rectangle dst = {trash_x, trash_y, (float)icon_size, (float)icon_size};

        if(mx > trash_x && mx < trash_x + icon_size && my > trash_y && my < trash_y + icon_size) {
            app->cursor_clickable = 1;
            DrawTexturePro(app->x_icon, src, dst, (Vector2){0}, 0, ui_darken(c_icon, 30));
            if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
                return 2;
        } else {
            DrawTexturePro(app->x_icon, src, dst, (Vector2){0}, 0, c_icon);
        }
    }

    if(hover && IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        return 1;
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
    snprintf(out, out_size, "%02d:%02d  avg %ds",
             entry->hour, entry->minute, entry->avg_seconds);
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

    if(count > 0 && has_day && app->history_level >= 3) {
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
    if(count > 0 && has_year && app->history_level >= 1) {
        content_rows += history_count_month_rows(entries, count, app->history_year);
        if(has_month && app->history_level >= 2) {
            content_rows += history_count_day_rows(entries, count, app->history_year, app->history_month);
            if(has_day && app->history_level >= 3) {
                content_rows += history_count_record_rows(entries, count,
                                                         app->history_year, app->history_month,
                                                         app->history_day);
                if(selected_index >= 0)
                    content_rows += entries[selected_index].round_count;
            }
        }
    }

    content_h = count > 0 ? count * row_h + 18 : viewport_h;
    if(content_rows > 0)
        content_h = 18 + content_rows * row_h;
    max_scroll = content_h - viewport_h;
    if(max_scroll < 0)
        max_scroll = 0;

    app->history_scroll -= (int)(GetMouseWheelMove() * 24.0f);
    app->history_scroll = (app->history_scroll < 0) ? 0 : (app->history_scroll > max_scroll ? max_scroll : app->history_scroll);

    ui_centered_column(CONTENT_MAX_W, CONTENT_SIDE_PAD, &content_x, &content_w);

    close_clicked = ui_draw_screen_header(app, "History", 1);
    if(close_clicked) {
        app->inbe.screen = InbeScreenStart;
        app->history_scroll = 0;
    }

    BeginScissorMode((int)app->camera.offset.x,
                     (int)(app->camera.offset.y + title_h * app->camera.zoom),
                     (int)(view_width * app->camera.zoom),
                     (int)(viewport_h * app->camera.zoom));
        y = title_h + ui_px(12) - app->history_scroll;
        if(count == 0) {
            int font = ui_clamp_px(14, 12, 16);
            const char *empty_text = "No saved sessions yet.\nComplete a session to add data.";
            TextLayout empty_layout = ui_text_layout_parse(empty_text, (Texture2D){0}, UI_ICON_TYPE_NONE, font);
            ui_text_layout_reflow(&empty_layout, content_w, font, ui_px(22));
            ui_text_layout_draw(&empty_layout, content_x, &y, font, c_text);
            ui_text_layout_free(&empty_layout);
        } else {
            int year = -1;
            int month = -1;
            int day = -1;

            for(int i = 0; i < count; i++) {
                char label[HISTORY_TEXT_SIZE];

                if(entries[i].year != year) {
                    int selected = app->history_year == entries[i].year && app->history_level >= 1;
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

                if(app->history_level < 1 || entries[i].year != app->history_year)
                    continue;

                if(entries[i].month != month) {
                    int selected = app->history_month == entries[i].month && app->history_level >= 2;
                    snprintf(label, sizeof(label), "Month %02d", entries[i].month);
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

                if(app->history_level < 2 || entries[i].month != app->history_month)
                    continue;

                if(entries[i].day != day) {
                    int selected = app->history_day == entries[i].day && app->history_level >= 3;
                    snprintf(label, sizeof(label), "Day %02d", entries[i].day);
                    if(draw_history_row(app, content_x, y, content_w, row_h, label, selected, 34)) {
                        app->history_day = entries[i].day;
                        history_clear_record_selection(app);
                        app->history_level = 3;
                        app->history_scroll = 0;
                    }
                    y += row_h;
                    day = entries[i].day;
                }

                if(app->history_level < 3 || entries[i].day != app->history_day)
                    continue;

                {
                    char time_label[HISTORY_TEXT_SIZE];
                    char record_name[16];
                    int selected;
                    int result;

                    snprintf(record_name, sizeof(record_name), "inbe-%02d%02d%02d",
                             entries[i].hour, entries[i].minute, entries[i].second);
                    history_format_session_label(&entries[i], time_label, sizeof(time_label));
                    selected = strcmp(app->history_record, record_name) == 0;
                    result = draw_history_session_row(app, content_x, y, content_w, row_h, time_label, selected);

                    if(result == 1) {
                        snprintf(app->history_record, sizeof(app->history_record), "inbe-%02d%02d%02d",
                                 entries[i].hour, entries[i].minute, entries[i].second);
                        app->history_level = 3;
                        selected_index = i;
                    } else if(result == 2) {
                        char dir_day[FS_PATH_MAX];
                        char path[FS_PATH_MAX];
                        snprintf(dir_day, sizeof(dir_day), "%s/%04d/%02d/%02d",
                                 data_root(), entries[i].year, entries[i].month, entries[i].day);
                        snprintf(path, sizeof(path), "%s/%s", dir_day, record_name);
                        remove(path);
                        scan_history_tree(entries, &count);
                        qsort(entries, (size_t)count, sizeof(entries[0]), compare_history_entries);
                        selected_index = -1;
                        app->history_record[0] = 0;
                        if(count == 0) {
                            app->history_level = 0;
                            app->history_year = 0;
                            app->history_month = 0;
                            app->history_day = 0;
                        }
                    }
                    y += row_h;

                    if(selected_index == i) {
                        for(int r = 0; r < entries[i].round_count; r++) {
                            char round_label[HISTORY_TEXT_SIZE];
                            history_format_round_label(&entries[i], r, round_label, sizeof(round_label));
                            if(draw_history_row(app, content_x, y, content_w, row_h, round_label, 0, 46)) {
                                /* Round clicked - could do something here */
                            }
                            y += row_h;
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
