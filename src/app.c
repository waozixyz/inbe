#define RINI_IMPLEMENTATION
#include "app.h"
#include "theme.h"
#include "../../vendor/rini/src/rini.h"
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define INBE_DEFAULT_TITLE "Inner Breeze"
#define INBE_DEFAULT_WIDTH 272
#define INBE_DEFAULT_HEIGHT 400
typedef struct InbeConfig {
    char title[64];
    int width;
    int height;
    int loaded;
} InbeConfig;

static InbeConfig config = {
    .title = INBE_DEFAULT_TITLE,
    .width = INBE_DEFAULT_WIDTH,
    .height = INBE_DEFAULT_HEIGHT,
    .loaded = 0
};

static int view_width = INBE_DEFAULT_WIDTH;
static int view_height = INBE_DEFAULT_HEIGHT;

static Color c_text, c_bg, c_circle, c_button, c_button_hover, c_icon;

enum {
    SETTINGS_SPEED_MIN = 1,
    SETTINGS_SPEED_MAX = 9,
    SETTINGS_BREATHS_MIN = 15,
    SETTINGS_BREATHS_MAX = 80,
    SETTINGS_PAUSE_MIN = 0,
    SETTINGS_PAUSE_MAX = 30,
    SETTINGS_TITLE_H = 38,
    TAB_BAR_H = 56,
    SETTINGS_CONTENT_H = 398,
    TUTORIAL_STEPS = 5,
    HISTORY_MAX_SESSIONS = 48,
    HISTORY_PATH_SIZE = 96,
    HISTORY_TEXT_SIZE = 96
};

typedef struct HistoryEntry {
    char path[HISTORY_PATH_SIZE];
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    int round_count;
    int avg_seconds;
    int rounds[MaxRounds];
} HistoryEntry;

static void save_session_results(InbeApp *app);
static void load_session_file(const char *path, HistoryEntry *entry);

static void
refresh_theme_colors(void)
{
    c_bg = theme_get("inbe", "background");
    c_text = theme_get("inbe", "text");
    c_circle = theme_get("inbe", "circle");
    c_button = theme_get("inbe", "button");
    c_button_hover = theme_get("inbe", "button_hover");
    c_icon = theme_get("inbe", "icon");
}

static void
load_config(void)
{
    if(config.loaded)
        return;

    const char *paths[] = {
        "inbe.ini",
        "apps/inbe.ini",
        "../inbe/inbe.ini",
        0
    };

    for(int i = 0; paths[i] != 0; i++) {
        rini_data ini = rini_load(paths[i]);
        if(ini.count == 0) {
            rini_unload(&ini);
            continue;
        }

        snprintf(config.title, sizeof(config.title), "%s",
                 rini_get_value_text_fallback(ini, "title", INBE_DEFAULT_TITLE));
        config.width = rini_get_value_fallback(ini, "width", INBE_DEFAULT_WIDTH);
        config.height = rini_get_value_fallback(ini, "height", INBE_DEFAULT_HEIGHT);
        rini_unload(&ini);
        break;
    }

    if(theme_scope("inbe") == NULL)
        theme_register_scope("inbe", "theme.ini");

    refresh_theme_colors();

    config.loaded = 1;
}

const char *
inbe_app_title(void)
{
    return config.title;
}

int
inbe_app_width(void)
{
    return config.width;
}

int
inbe_app_height(void)
{
    return config.height;
}

static void
draw_bevel(int x, int y, int w, int h, Color light, Color dark)
{
    DrawLine(x, y, x + w - 1, y, light);
    DrawLine(x, y, x, y + h - 1, light);
    DrawLine(x + w - 1, y, x + w - 1, y + h - 1, dark);
    DrawLine(x, y + h - 1, x + w - 1, y + h - 1, dark);
}

static Color
lighten(Color c, int amount)
{
    return (Color){
        (unsigned char)(c.r + amount > 255 ? 255 : c.r + amount),
        (unsigned char)(c.g + amount > 255 ? 255 : c.g + amount),
        (unsigned char)(c.b + amount > 255 ? 255 : c.b + amount),
        c.a
    };
}

static Color
darken(Color c, int amount)
{
    return (Color){
        (unsigned char)(c.r < amount ? 0 : c.r - amount),
        (unsigned char)(c.g < amount ? 0 : c.g - amount),
        (unsigned char)(c.b < amount ? 0 : c.b - amount),
        c.a
    };
}

static int
clampi(int value, int min, int max)
{
    if(value < min)
        return min;
    if(value > max)
        return max;
    return value;
}

static void
count_from_int(char dst[CountSize], int value)
{
    value = clampi(value, 0, 999);
    dst[0] = (char)('0' + (value / 100) % 10);
    dst[1] = (char)('0' + (value / 10) % 10);
    dst[2] = (char)('0' + value % 10);
    dst[3] = 0;
}

static int
int_from_count(const char src[CountSize])
{
    int a = (src[0] >= '0' && src[0] <= '9') ? src[0] - '0' : 0;
    int b = (src[1] >= '0' && src[1] <= '9') ? src[1] - '0' : 0;
    int c = (src[2] >= '0' && src[2] <= '9') ? src[2] - '0' : 0;
    return a * 100 + b * 10 + c;
}

static void
reset_round_breathe(Inbe *inbe)
{
    inbe->phase = InbePhaseBreathe;
    inbe->dir = 0;
    inbe->r = inbe->rmin;
    inbe->breath_frame = 0;
    inbe->breathtick = 0;
    inbe->sectick = 0;
    inbe->halftick = 0;
    cpcount(inbe->count, "000");
}

static void
reset_round_start(Inbe *inbe)
{
    reset_round_breathe(inbe);
    if(inbe->pause_seconds > 0)
        inbe->phase = InbePhaseStarting;
}

static void
reset_round_recover(Inbe *inbe)
{
    inbe->phase = InbePhaseRecover;
    inbe->r = inbe->rmin;
    inbe->breath_frame = 0;
    inbe->breathtick = 0;
    inbe->sectick = 0;
    inbe->halftick = 0;
    cpcount(inbe->count, "000");
}

static void
apply_settings(Inbe *inbe, int speed, int max_rounds, int max_breaths, int pause_seconds)
{
    static const int breath_half_ticks[] = {180, 156, 132, 114, 96, 78, 66, 54, 45};
    static const int recover_pixels[] = {1, 1, 2, 2, 3, 3, 4, 4, 5};
    static const int recover_ticks[] = {4, 3, 5, 4, 5, 4, 5, 4, 4};

    speed = clampi(speed, SETTINGS_SPEED_MIN, SETTINGS_SPEED_MAX);
    inbe->speed_level = speed;
    inbe->speed = recover_pixels[speed - 1];
    inbe->breathtickmax = recover_ticks[speed - 1];
    inbe->breath_half_ticks = breath_half_ticks[speed - 1];
    inbe->max_rounds = clampi(max_rounds, 1, MaxRounds);
    inbe->pause_seconds = clampi(pause_seconds, SETTINGS_PAUSE_MIN, SETTINGS_PAUSE_MAX);
    count_from_int(inbe->maxbreaths, clampi(max_breaths, SETTINGS_BREATHS_MIN, SETTINGS_BREATHS_MAX));
}

static void
reset_settings_preview(InbeApp *app)
{
    int speed = app->inbe.speed_level;
    int max_rounds = app->inbe.max_rounds;
    int max_breaths = int_from_count(app->inbe.maxbreaths);
    int pause_seconds = app->inbe.pause_seconds;

    inbeinit(&app->settings_preview);
    apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
    app->settings_preview.rmin = view_width * 0.2f;
    app->settings_preview.rmax = view_width * 0.4f;
    reset_round_breathe(&app->settings_preview);
}

static void
save_settings(InbeApp *app)
{
    char text[160];
    snprintf(text, sizeof(text),
             "speed %d\nmax_rounds %d\nmax_breaths %d\npause_seconds %d\ntutorial_seen %d\n",
             app->inbe.speed_level,
             app->inbe.max_rounds,
             int_from_count(app->inbe.maxbreaths),
             app->inbe.pause_seconds,
             app->tutorial_seen ? 1 : 0);
    SaveFileText("settings.ini", text);
    app->settings_dirty = 0;
}

static void
load_settings(InbeApp *app)
{
    rini_data settings = rini_load("settings.ini");

    int speed = rini_get_value_fallback(settings, "speed", 6);
    int max_rounds = rini_get_value_fallback(settings, "max_rounds", DefaultMaxRounds);
    int max_breaths = rini_get_value_fallback(settings, "max_breaths", DefaultMaxBreaths);
    int pause_seconds = rini_get_value_fallback(settings, "pause_seconds", DefaultPauseSeconds);

    app->tutorial_seen = rini_get_value_fallback(settings, "tutorial_seen", 0) != 0;
    apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
    rini_unload(&settings);
}

static Texture2D
load_pixel_texture(const char *path)
{
    Texture2D texture = LoadTexture(path);
    if(texture.id != 0)
        SetTextureFilter(texture, TEXTURE_FILTER_POINT);
    return texture;
}

static Texture2D
load_icon_texture(const char *name)
{
    char path[64];

    snprintf(path, sizeof(path), "icons/%s", name);
#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID) || defined(PLATFORM_WEB)
    return load_pixel_texture(path);
#else
    if(FileExists(path))
        return load_pixel_texture(path);

    snprintf(path, sizeof(path), "../icons/%s", name);
    return load_pixel_texture(path);
#endif
}

static Texture2D
load_asset_texture(const char *name)
{
    char path[64];

    snprintf(path, sizeof(path), "assets/%s", name);
#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID) || defined(PLATFORM_WEB)
    return load_pixel_texture(path);
#else
    if(FileExists(path))
        return load_pixel_texture(path);

    snprintf(path, sizeof(path), "../inbe/assets/%s", name);
    if(FileExists(path))
        return load_pixel_texture(path);

    snprintf(path, sizeof(path), "../assets/%s", name);
    return load_pixel_texture(path);
#endif
}

static void
start_session(InbeApp *app)
{
    int speed = app->inbe.speed_level;
    int max_rounds = app->inbe.max_rounds;
    int max_breaths = int_from_count(app->inbe.maxbreaths);
    int pause_seconds = app->inbe.pause_seconds;

    inbeinit(&app->inbe);
    app->inbe.rmax = view_width * 0.4f;
    app->inbe.rmin = view_width * 0.2f;
    app->inbe.r = app->inbe.rmin;
    apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
    app->inbe.screen = InbeScreenSession;
    app->session_paused = 0;
    app->results_saved = 0;
}

static void
finish_hold(InbeApp *app)
{
    cpcount(app->inbe.results[app->inbe.round], app->inbe.count);
    cpcount(app->inbe.count, "000");
    app->inbe.phase = InbePhaseRecover;
    app->inbe.r = app->inbe.rmin;
    app->inbe.breath_frame = 0;
    app->inbe.breathtick = 0;
    app->inbe.sectick = 0;
}

static void
finish_round(InbeApp *app)
{
    app->inbe.breathtick = 0;
    app->inbe.sectick = 0;
    cpcount(app->inbe.count, "000");

    if(app->inbe.round < app->inbe.max_rounds - 1) {
        app->inbe.round++;
        reset_round_start(&app->inbe);
    } else {
        save_session_results(app);
        app->inbe.screen = InbeScreenResults;
    }
}

static void
session_step_back(InbeApp *app)
{
    switch(app->inbe.phase) {
    case InbePhaseStarting:
        if(app->inbe.round > 0) {
            app->inbe.round--;
            reset_round_recover(&app->inbe);
        } else {
            reset_round_start(&app->inbe);
        }
        break;
    case InbePhaseBreathe:
        if(app->inbe.pause_seconds > 0) {
            reset_round_start(&app->inbe);
        } else if(app->inbe.round > 0) {
            app->inbe.round--;
            reset_round_recover(&app->inbe);
        } else {
            reset_round_breathe(&app->inbe);
        }
        break;
    case InbePhaseHold:
        app->inbe.phase = InbePhaseBreathe;
        app->inbe.r = app->inbe.rmin;
        app->inbe.breath_frame = 0;
        app->inbe.breathtick = 0;
        app->inbe.sectick = 0;
        cpcount(app->inbe.count, "000");
        break;
    case InbePhaseRecover:
    case InbePhaseNext:
        app->inbe.phase = InbePhaseRecover;
        app->inbe.r = app->inbe.rmax;
        app->inbe.breath_frame = 0;
        app->inbe.sectick = 0;
        cpcount(app->inbe.count, "000");
        break;
    }
}

static void
session_step_forward(InbeApp *app)
{
    switch(app->inbe.phase) {
    case InbePhaseStarting:
        reset_round_breathe(&app->inbe);
        break;
    case InbePhaseBreathe:
        app->inbe.phase = InbePhaseHold;
        app->inbe.r = app->inbe.rmin;
        app->inbe.breath_frame = 0;
        app->inbe.breathtick = 0;
        app->inbe.sectick = 0;
        cpcount(app->inbe.count, "000");
        break;
    case InbePhaseHold:
        finish_hold(app);
        break;
    case InbePhaseRecover:
        app->inbe.phase = InbePhaseNext;
        app->inbe.breath_frame = 0;
        app->inbe.sectick = 0;
        cpcount(app->inbe.count, "000");
        break;
    case InbePhaseNext:
        finish_round(app);
        break;
    }
}

static void
ensure_dir(const char *path)
{
    if(!DirectoryExists(path))
        MakeDirectory(path);
}

static const char *
history_root(void)
{
    static char root[PATH_MAX];

#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
    return "data";
#else
    const char *xdg = getenv("XDG_DATA_HOME");
    const char *home = getenv("HOME");

    if(root[0] != '\0')
        return root;
    if(xdg != NULL && xdg[0] != '\0')
        snprintf(root, sizeof(root), "%s/lotus/home", xdg);
    else if(home != NULL && home[0] != '\0')
        snprintf(root, sizeof(root), "%s/.local/share/lotus/home", home);
    else
        snprintf(root, sizeof(root), ".local/lotus/home");
    return root;
#endif
}

static void
save_session_results(InbeApp *app)
{
    time_t now;
    struct tm *tm;
    char dir_year[32];
    char dir_month[48];
    char dir_day[64];
    char path[96];
    char text[MaxRounds * 8];
    int offset = 0;
    int played_rounds;

    if(app->results_saved)
        return;

    now = time(NULL);
    tm = localtime(&now);
    if(tm == NULL)
        return;

    played_rounds = app->inbe.round + 1;
    if(played_rounds < 1)
        played_rounds = 1;
    if(played_rounds > app->inbe.max_rounds)
        played_rounds = app->inbe.max_rounds;

    ensure_dir(history_root());
    snprintf(dir_year, sizeof(dir_year), "%s/%04d", history_root(), tm->tm_year + 1900);
    ensure_dir(dir_year);
    snprintf(dir_month, sizeof(dir_month), "%s/%02d", dir_year, tm->tm_mon + 1);
    ensure_dir(dir_month);
    snprintf(dir_day, sizeof(dir_day), "%s/%02d", dir_month, tm->tm_mday);
    ensure_dir(dir_day);
    snprintf(path, sizeof(path), "%s/inbe-%02d%02d%02d",
             dir_day, tm->tm_hour, tm->tm_min, tm->tm_sec);

    for(int i = 0; i < played_rounds && i < MaxRounds; i++) {
        int seconds = int_from_count(app->inbe.results[i]);
        if(offset >= (int)sizeof(text))
            break;
        offset += snprintf(text + offset, sizeof(text) - (size_t)offset, "%d\n", seconds);
    }

    ensure_dir(history_root());
    TraceLog(LOG_INFO, "INBE: saving results to %s", path);
    if(SaveFileText(path, text)) {
        TraceLog(LOG_INFO, "INBE: saved results to %s", path);
        app->results_saved = 1;
    } else {
        TraceLog(LOG_WARNING, "INBE: failed to save results to %s", path);
    }
}

static void
prepare_history_storage(void)
{
    time_t now;
    struct tm *tm;
    char dir_year[32];
    char dir_month[48];
    char dir_day[64];

    now = time(NULL);
    tm = localtime(&now);
    if(tm == NULL)
        return;

    ensure_dir(history_root());
    snprintf(dir_year, sizeof(dir_year), "%s/%04d", history_root(), tm->tm_year + 1900);
    ensure_dir(dir_year);
    snprintf(dir_month, sizeof(dir_month), "%s/%02d", dir_year, tm->tm_mon + 1);
    ensure_dir(dir_month);
    snprintf(dir_day, sizeof(dir_day), "%s/%02d", dir_month, tm->tm_mday);
    ensure_dir(dir_day);
}

static void
add_history_entry(HistoryEntry *entries, int *count, int year, int month, int day, const char *path)
{
    const char *name;
    int hh = 0;
    int mm = 0;
    int ss = 0;
    HistoryEntry entry;

    if(*count >= HISTORY_MAX_SESSIONS)
        return;

    name = strrchr(path, '/');
    name = name != NULL ? name + 1 : path;
    if(sscanf(name, "inbe-%2d%2d%2d", &hh, &mm, &ss) != 3)
        return;

    memset(&entry, 0, sizeof(entry));
    snprintf(entry.path, sizeof(entry.path), "%s", path);
    entry.year = year;
    entry.month = month;
    entry.day = day;
    entry.hour = hh;
    entry.minute = mm;
    entry.second = ss;
    load_session_file(path, &entry);
    if(entry.round_count <= 0)
        return;
    entries[*count] = entry;
    (*count)++;
}

static int
compare_history_entries(const void *a, const void *b)
{
    const HistoryEntry *ea = a;
    const HistoryEntry *eb = b;

    return strcmp(eb->path, ea->path);
}

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

static void
scan_history_day(HistoryEntry *entries, int *count, int year, int month, int day, const char *path)
{
    DIR *dir = opendir(path);
    struct dirent *ent;
    char child[HISTORY_PATH_SIZE];

    if(dir == NULL)
        return;

    while((ent = readdir(dir)) != NULL && *count < HISTORY_MAX_SESSIONS) {
        if(ent->d_name[0] == '.')
            continue;
        snprintf(child, sizeof(child), "%s/%s", path, ent->d_name);
        if(strncmp(ent->d_name, "inbe-", 5) == 0)
            add_history_entry(entries, count, year, month, day, child);
    }

    closedir(dir);
}

static void
scan_history_tree(HistoryEntry *entries, int *count)
{
    DIR *years = opendir(history_root());
    struct dirent *year;
    char ypath[HISTORY_PATH_SIZE];
    char mpath[HISTORY_PATH_SIZE];
    char dpath[HISTORY_PATH_SIZE];

    *count = 0;
    if(years == NULL)
        return;

    while((year = readdir(years)) != NULL && *count < HISTORY_MAX_SESSIONS) {
        if(!name_is_digits(year->d_name, 4))
            continue;
        snprintf(ypath, sizeof(ypath), "%s/%s", history_root(), year->d_name);
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

static int
history_has_match(const HistoryEntry *entries, int count, int year, int month, int day)
{
    for(int i = 0; i < count; i++) {
        if(entries[i].year == year && entries[i].month == month && entries[i].day == day)
            return 1;
    }
    return 0;
}

static int
history_has_year(const HistoryEntry *entries, int count, int year)
{
    for(int i = 0; i < count; i++) {
        if(entries[i].year == year)
            return 1;
    }
    return 0;
}

static int
history_has_month(const HistoryEntry *entries, int count, int year, int month)
{
    for(int i = 0; i < count; i++) {
        if(entries[i].year == year && entries[i].month == month)
            return 1;
    }
    return 0;
}

static int
history_has_day_only(const HistoryEntry *entries, int count, int year, int month, int day)
{
    return history_has_match(entries, count, year, month, day);
}

static void
history_set_selected_record(InbeApp *app, const HistoryEntry *entry)
{
    snprintf(app->history_record, sizeof(app->history_record), "inbe-%02d%02d%02d",
             entry->hour, entry->minute, entry->second);
}

static void
history_set_selection(InbeApp *app, const HistoryEntry *entry, int level)
{
    app->history_year = entry->year;
    app->history_month = entry->month;
    app->history_day = entry->day;
    app->history_level = level;
    history_set_selected_record(app, entry);
}

static void
history_clear_record_selection(InbeApp *app)
{
    app->history_record[0] = 0;
}

static void
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
                    history_set_selection(app, &entries[i], 3);
                    app->history_scroll = 0;
                    return;
                }
            }
        }

        history_set_selection(app, &entries[0], 3);
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

static int
draw_history_row(InbeApp *app, int x, int y, int w, int h, const char *text, int selected, int indent)
{
    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
    int mx = (int)mouse_world.x;
    int my = (int)mouse_world.y;
    int hover = 0;

    if(mx > x && mx < x + w && my > y && my < y + h) {
        DrawRectangle(x, y, w, h, selected ? c_button_hover : darken(c_button_hover, 6));
        draw_bevel(x, y, w, h, darken(c_button_hover, 40), lighten(c_button_hover, 40));
        hover = 1;
        app->cursor_clickable = 1;
        if(IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            draw_bevel(x, y, w, h, lighten(c_button_hover, 40), darken(c_button_hover, 40));
        }
    } else {
        DrawRectangle(x, y, w, h, selected ? c_button : darken(c_bg, 6));
        draw_bevel(x, y, w, h, lighten(c_button, 28), darken(c_button, 20));
    }

    DrawText(text, x + indent, y + 6, 14, c_text);
    return hover && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

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

static void
load_session_file(const char *path, HistoryEntry *entry)
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

static void
history_format_session_label(const HistoryEntry *entry, char *out, int out_size)
{
    if(out == NULL || out_size <= 0)
        return;

    snprintf(out, (size_t)out_size, "%02d:%02d:%02d  avg %ds",
             entry->hour, entry->minute, entry->second, entry->avg_seconds);
}

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

static int
drawbtn(InbeApp *app, int x, int y, const char *label, int *hover)
{
    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
    int mx = (int)mouse_world.x;
    int my = (int)mouse_world.y;

    int mb = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    int released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    int font = 20;
    int w = (int)MeasureText(label, font) + 20;
    int h = 30;

    x = x - w / 2;

    int pressed = 0;

    if(mx > x && mx < x + w && my > y && my < y + h) {
        DrawRectangle(x, y, w, h, c_button_hover);
        draw_bevel(x, y, w, h, darken(c_button_hover, 40), lighten(c_button_hover, 40));
        *hover = 1;
        app->cursor_clickable = 1;
        if(mb) {
            draw_bevel(x, y, w, h, lighten(c_button_hover, 40), darken(c_button_hover, 40));
        }
        if(released) {
            pressed = 1;
        }
    } else {
        DrawRectangle(x, y, w, h, c_button);
        draw_bevel(x, y, w, h, lighten(c_button, 40), darken(c_button, 40));
        *hover = 0;
    }

    DrawText(label, x + 10, y + 5, font, c_text);

    return pressed;
}

static int
drawiconbtn(InbeApp *app, int x, int y, int size, Texture2D icon, int *hover)
{
    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
    int mx = (int)mouse_world.x;
    int my = (int)mouse_world.y;

    int mb = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    int released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    int w = size + 8;
    int h = size + 8;

    int pressed = 0;

    if(mx > x && mx < x + w && my > y && my < y + h) {
        DrawRectangle(x, y, w, h, c_button_hover);
        draw_bevel(x, y, w, h, darken(c_button_hover, 40), lighten(c_button_hover, 40));
        *hover = 1;
        app->cursor_clickable = 1;
        if(mb) {
            draw_bevel(x, y, w, h, lighten(c_button_hover, 40), darken(c_button_hover, 40));
        }
        if(released) {
            pressed = 1;
        }
    } else {
        DrawRectangle(x, y, w, h, c_button);
        draw_bevel(x, y, w, h, lighten(c_button, 40), darken(c_button, 40));
        *hover = 0;
    }

    if(icon.id != 0) {
        Rectangle src = {0, 0, icon.width, icon.height};
        Rectangle dst = {x + 4, y + 4, (float)size, (float)size};
        DrawTexturePro(icon, src, dst, (Vector2){0}, 0, c_icon);
    }

    return pressed;
}

static void
draw_tab_bar(InbeApp *app)
{
    int bar_y = view_height - TAB_BAR_H;
    int button_size = 32;
    int button_w = button_size + 12;
    int gap = 16;
    int total_w = button_w * 3 + gap * 2;
    int start_x = view_width / 2 - total_w / 2;
    int stat_x = start_x;
    int manual_x = stat_x + button_w + gap;
    int gear_x = manual_x + button_w + gap;
    int tab_hover = 0;

    DrawRectangle(0, bar_y, view_width, TAB_BAR_H, darken(c_bg, 10));
    DrawLine(0, bar_y, view_width, bar_y, darken(c_bg, 42));

    if(app->stat_icon.id != 0) {
        if(drawiconbtn(app, stat_x, bar_y + 10, button_size, app->stat_icon, &tab_hover)) {
            history_open_latest(app);
            app->inbe.screen = InbeScreenHistory;
        }
    }
    if(app->manual_icon.id != 0) {
        if(drawiconbtn(app, manual_x, bar_y + 10, button_size, app->manual_icon, &tab_hover)) {
            app->tutorial_step = 0;
            app->inbe.screen = InbeScreenManual;
        }
    }
    if(app->gear_icon.id != 0) {
        if(drawiconbtn(app, gear_x, bar_y + 10, button_size, app->gear_icon, &tab_hover)) {
            reset_settings_preview(app);
            app->inbe.screen = InbeScreenSettings;
        }
    }
}

static void
draw_session_counter(InbeApp *app, int center_x, int center_y);

static void
drawinbe(InbeApp *app, int center_x, int center_y)
{
    DrawCircle(center_x, center_y, app->inbe.r, c_circle);
    DrawCircleLines(center_x, center_y, app->inbe.r, c_text);
    draw_session_counter(app, center_x, center_y);
}

static void
draw_session_status(InbeApp *app, int center_x, int center_y)
{
    char text[32];
    int remaining;
    int text_w;

    if(app->inbe.phase != InbePhaseStarting || app->inbe.pause_seconds <= 0)
        return;

    remaining = app->inbe.pause_seconds - app->inbe.sectick / 60;
    if(remaining < 1)
        remaining = 1;

    snprintf(text, sizeof(text), "STARTING IN %d", remaining);
    text_w = MeasureText(text, 18);
    DrawText(text, center_x - text_w / 2, center_y + app->inbe.rmin + 12, 18, c_text);
}

static void
draw_session_counter(InbeApp *app, int center_x, int center_y)
{
    char text[CountSize];
    int count;
    int text_w;

    if(app->inbe.phase == InbePhaseRecover) {
        if(app->inbe.r < app->inbe.rmax) {
            DrawText("000", center_x - MeasureText("000", 20) / 2, center_y - 10, 20, c_text);
            return;
        }

        count = int_from_count(app->inbe.count);
        if(count < 15) {
            count_from_int(text, 15 - count);
            text_w = MeasureText(text, 20);
            DrawText(text, center_x - text_w / 2, center_y - 10, 20, c_text);
            return;
        }
        DrawText("000", center_x - MeasureText("000", 20) / 2, center_y - 10, 20, c_text);
        return;
    }

    if(app->inbe.phase == InbePhaseNext) {
        DrawText("000", center_x - MeasureText("000", 20) / 2, center_y - 10, 20, c_text);
        return;
    }

    text_w = MeasureText(app->inbe.count, 20);
    DrawText(app->inbe.count, center_x - text_w / 2, center_y - 10, 20, c_text);
}

static void
draw_preview_inbe(Inbe *inbe, int center_x, int center_y)
{
    int r = (int)((float)inbe->r * 0.48f);
    DrawCircle(center_x, center_y, r, c_circle);
    DrawCircleLines(center_x, center_y, r, c_text);
}

static int
draw_slider(InbeApp *app, int id, int x, int y, int w, const char *label,
            int min, int max, int *value, const char *suffix)
{
    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
    int mx = (int)mouse_world.x;
    int label_font = 16;
    int value_font = 16;
    int track_y = y + 28;
    int track_h = 8;
    int knob_w = 12;
    int knob_h = 22;
    int changed = 0;
    char value_text[32];
    Rectangle hit = {(float)(x - 6), (float)(track_y - 10), (float)(w + 12), 32};

    snprintf(value_text, sizeof(value_text), "%d%s", *value, suffix != NULL ? suffix : "");
    DrawText(label, x, y, label_font, c_text);
    DrawText(value_text, x + w - MeasureText(value_text, value_font), y, value_font, c_text);

    DrawRectangle(x, track_y, w, track_h, darken(c_bg, 28));
    draw_bevel(x, track_y, w, track_h, darken(c_bg, 55), lighten(c_bg, 35));

    if(CheckCollisionPointRec(mouse_world, hit)) {
        app->cursor_clickable = 1;
        if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            app->settings_drag_slider = id;
    }

    if(app->settings_drag_slider == id && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        int old_value = *value;
        float t = (float)(mx - x) / (float)w;
        if(t < 0.0f)
            t = 0.0f;
        if(t > 1.0f)
            t = 1.0f;
        *value = min + (int)(t * (float)(max - min) + 0.5f);
        *value = clampi(*value, min, max);
        changed = (*value != old_value);
    }

    float t = (float)(*value - min) / (float)(max - min);
    int knob_x = x + (int)(t * (float)w) - knob_w / 2;
    DrawRectangle(knob_x, track_y - 7, knob_w, knob_h, c_button);
    draw_bevel(knob_x, track_y - 7, knob_w, knob_h, lighten(c_button, 40), darken(c_button, 40));

    return changed;
}

static void
draw_scrollbar(InbeApp *app, int *scroll, int content_h, int viewport_h)
{
    if(content_h <= viewport_h)
        return;

    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
    int bar_x = view_width - 8;
    int bar_y = SETTINGS_TITLE_H + 4;
    int bar_w = 4;
    int bar_h = viewport_h - 8;
    int thumb_h = (viewport_h * bar_h) / content_h;
    if(thumb_h < 24)
        thumb_h = 24;
    int max_scroll = content_h - viewport_h;
    int thumb_y = bar_y + (*scroll * (bar_h - thumb_h)) / max_scroll;
    Rectangle thumb = {(float)(bar_x - 3), (float)thumb_y, 10, (float)thumb_h};

    DrawRectangle(bar_x, bar_y, bar_w, bar_h, darken(c_bg, 18));
    DrawRectangle(bar_x - 1, thumb_y, bar_w + 2, thumb_h, c_button_hover);

    if(CheckCollisionPointRec(mouse_world, thumb)) {
        app->cursor_clickable = 1;
        if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            app->settings_drag_scrollbar = 1;
    }

    if(app->settings_drag_scrollbar && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        int usable = bar_h - thumb_h;
        int y = (int)mouse_world.y - bar_y - thumb_h / 2;
        y = clampi(y, 0, usable);
        *scroll = (y * max_scroll) / usable;
    }
}

static void
draw_settings(InbeApp *app)
{
    int center_x = view_width / 2;
    int viewport_h = view_height - SETTINGS_TITLE_H - TAB_BAR_H;
    int max_scroll = SETTINGS_CONTENT_H - viewport_h;
    int gear_hover = 0;

    if(max_scroll < 0)
        max_scroll = 0;

    app->settings_scroll -= (int)(GetMouseWheelMove() * 24.0f);
    app->settings_scroll = clampi(app->settings_scroll, 0, max_scroll);

    DrawRectangle(0, 0, view_width, SETTINGS_TITLE_H, darken(c_bg, 14));
    DrawLine(0, SETTINGS_TITLE_H - 1, view_width, SETTINGS_TITLE_H - 1, darken(c_bg, 42));
    DrawText("Settings", 12, 11, 18, c_text);

    if(drawiconbtn(app, view_width - 34, 7, 16, app->x_icon, &gear_hover)) {
        if(app->settings_dirty)
            save_settings(app);
        app->inbe.screen = InbeScreenStart;
        app->settings_scroll = 0;
    }

    BeginScissorMode((int)app->camera.offset.x,
                     (int)(app->camera.offset.y + SETTINGS_TITLE_H * app->camera.zoom),
                     (int)(view_width * app->camera.zoom),
                     (int)(viewport_h * app->camera.zoom));
        int yoff = SETTINGS_TITLE_H - app->settings_scroll;
        int speed = app->inbe.speed_level;
        int max_rounds = app->inbe.max_rounds;
        int max_breaths = int_from_count(app->inbe.maxbreaths);
        int pause_seconds = app->inbe.pause_seconds;

        apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
        inbestep(&app->settings_preview);
        if(app->settings_preview.phase != InbePhaseBreathe) {
            reset_settings_preview(app);
            apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
        }

        draw_preview_inbe(&app->settings_preview, center_x, yoff + 48);

        if(draw_slider(app, 1, 28, yoff + 104, 184, "Speed", SETTINGS_SPEED_MIN,
                       SETTINGS_SPEED_MAX, &speed, "")) {
            apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
            apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
            app->settings_dirty = 1;
        }

        if(draw_slider(app, 2, 28, yoff + 170, 184, "Max rounds", 1,
                       MaxRounds, &max_rounds, "")) {
            apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
            apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
            app->settings_dirty = 1;
        }

        if(draw_slider(app, 3, 28, yoff + 236, 184, "Max breaths", SETTINGS_BREATHS_MIN,
                       SETTINGS_BREATHS_MAX, &max_breaths, "")) {
            apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
            apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
            app->settings_dirty = 1;
        }

        if(draw_slider(app, 4, 28, yoff + 302, 184, "Pause after round", SETTINGS_PAUSE_MIN,
                       SETTINGS_PAUSE_MAX, &pause_seconds, "s")) {
            apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
            apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
            app->settings_dirty = 1;
        }
    EndScissorMode();

    draw_scrollbar(app, &app->settings_scroll, SETTINGS_CONTENT_H, viewport_h);
    draw_tab_bar(app);

    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        app->settings_drag_slider = 0;
        app->settings_drag_scrollbar = 0;
        if(app->settings_dirty)
            save_settings(app);
    }
}

static void
tutorial_close(InbeApp *app, int mark_seen)
{
    if(mark_seen && !app->tutorial_seen) {
        app->tutorial_seen = 1;
        save_settings(app);
    }
    app->tutorial_step = 0;
    app->manual_scroll = 0;
    app->inbe.screen = InbeScreenStart;
}

static void
draw_text_lines(const char **lines, int count, int x, int *y, int font, int line_h)
{
    for(int i = 0; i < count; i++) {
        DrawText(lines[i], x, *y, font, c_text);
        *y += line_h;
    }
}

static void
draw_tutorial_image_placeholder(const char *label, int x, int y, int w, int h)
{
    DrawRectangle(x, y, w, h, darken(c_bg, 12));
    draw_bevel(x, y, w, h, darken(c_bg, 45), lighten(c_bg, 35));
    int tw = MeasureText(label, 14);
    DrawText(label, x + w / 2 - tw / 2, y + h / 2 - 7, 14, c_text);
}

static void
draw_tutorial_image(Texture2D texture, const char *fallback, int x, int y, int w, int h)
{
    if(texture.id == 0) {
        draw_tutorial_image_placeholder(fallback, x, y, w, h);
        return;
    }

    float scale_x = (float)w / (float)texture.width;
    float scale_y = (float)h / (float)texture.height;
    float scale = scale_x < scale_y ? scale_x : scale_y;
    float dst_w = (float)texture.width * scale;
    float dst_h = (float)texture.height * scale;
    Rectangle src = {0, 0, (float)texture.width, (float)texture.height};
    Rectangle dst = {x + ((float)w - dst_w) * 0.5f, y + ((float)h - dst_h) * 0.5f, dst_w, dst_h};

    DrawRectangle(x, y, w, h, darken(c_bg, 12));
    draw_bevel(x, y, w, h, darken(c_bg, 45), lighten(c_bg, 35));
    DrawTexturePro(texture, src, dst, (Vector2){0}, 0, WHITE);
}

static void
draw_manual(InbeApp *app)
{
    int x = 14;
    int viewport_h = view_height - SETTINGS_TITLE_H - TAB_BAR_H;
    int content_h = 430;
    int title_font = 18;
    int body_font = 14;
    int title_w;
    int previous_step;
    int max_scroll;
    const char *title = "Tutorial";
    int footer_y = view_height - TAB_BAR_H - 46;
    int footer_mid_y = footer_y + 7;
    char page_label[16];

    app->tutorial_step = clampi(app->tutorial_step, 0, TUTORIAL_STEPS - 1);
    previous_step = app->tutorial_step;

    if(IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_ENTER)) {
        if(app->tutorial_step < TUTORIAL_STEPS - 1)
            app->tutorial_step++;
        else
            tutorial_close(app, 1);
    }
    if(IsKeyPressed(KEY_LEFT) && app->tutorial_step > 0)
        app->tutorial_step--;
    if(IsKeyPressed(KEY_ESCAPE))
        tutorial_close(app, 1);

    if(previous_step != app->tutorial_step) {
        app->manual_scroll = 0;
        previous_step = app->tutorial_step;
    }

    switch(app->tutorial_step) {
    case 1: title = "Method"; content_h = 275; break;
    case 2: title = "Step 1: In & Out"; content_h = 315; break;
    case 3: title = "Step 2: Exhale & Hold"; content_h = 205; break;
    case 4: title = "Step 3: Inhale & Hold"; content_h = 440; break;
    default: break;
    }
    content_h += 28;

    max_scroll = content_h - viewport_h;
    if(max_scroll < 0)
        max_scroll = 0;
    app->manual_scroll -= (int)(GetMouseWheelMove() * 24.0f);
    app->manual_scroll = clampi(app->manual_scroll, 0, max_scroll);

    DrawRectangle(0, 0, view_width, SETTINGS_TITLE_H, darken(c_bg, 14));
    DrawLine(0, SETTINGS_TITLE_H - 1, view_width, SETTINGS_TITLE_H - 1, darken(c_bg, 42));
    title_w = MeasureText(title, title_font);
    DrawText(title, view_width / 2 - title_w / 2, 11, title_font, c_text);

    int x_hover = 0;
    if(drawiconbtn(app, view_width - 34, 7, 16, app->x_icon, &x_hover))
        tutorial_close(app, 1);

    BeginScissorMode((int)app->camera.offset.x,
                     (int)(app->camera.offset.y + SETTINGS_TITLE_H * app->camera.zoom),
                     (int)(view_width * app->camera.zoom),
                     (int)(viewport_h * app->camera.zoom));
        int y = SETTINGS_TITLE_H + 16 - app->manual_scroll;
        if(app->tutorial_step == 0) {
            const char *lines[] = {
                "This breathing practice can be",
                "powerful. Use it with care.",
                "",
                "Practice sitting or lying down.",
                "Never use it while driving,",
                "standing, or in water."
            };
            draw_tutorial_image(app->angel_image, "angel.png", 18, y, view_width - 36, 240);
            y += 262;
            draw_text_lines(lines, 6, x, &y, body_font, 20);
        } else if(app->tutorial_step == 1) {
            const char *lines[] = {
                "Simply follow 4 steps:",
                "",
                "1. Breathe rhythmically.",
                "2. Exhale and hold.",
                "3. Inhale deeply and hold.",
                "4. Exhale and repeat.",
                "",
                "Use the gear icon on the",
                "title screen to adjust rounds,",
                "breaths, speed, and pauses."
            };
            draw_text_lines(lines, 10, x, &y, body_font, 19);
            if(app->gear_icon.id != 0) {
                int gear_hover = 0;
                DrawText("Settings", x, y + 7, 14, c_text);
                drawiconbtn(app, x + 80, y, 16, app->gear_icon, &gear_hover);
            }
        } else if(app->tutorial_step == 2) {
            int speed = app->inbe.speed_level;
            const char *lines[] = {
                "Fill your lungs fully, then",
                "let the breath flow out.",
                "",
                "Use this slider to set the",
                "pace of the breathing circle."
            };
            draw_text_lines(lines, 5, x, &y, body_font, 19);
            y += 8;

            apply_settings(&app->settings_preview, speed, app->inbe.max_rounds,
                           int_from_count(app->inbe.maxbreaths), app->inbe.pause_seconds);
            inbestep(&app->settings_preview);
            if(app->settings_preview.phase != InbePhaseBreathe) {
                reset_settings_preview(app);
                apply_settings(&app->settings_preview, speed, app->inbe.max_rounds,
                               int_from_count(app->inbe.maxbreaths), app->inbe.pause_seconds);
            }
            draw_preview_inbe(&app->settings_preview, view_width / 2, y + 40);
            y += 86;

            if(draw_slider(app, 10, 28, y, 184, "Speed", SETTINGS_SPEED_MIN,
                           SETTINGS_SPEED_MAX, &speed, "")) {
                apply_settings(&app->inbe, speed, app->inbe.max_rounds,
                               int_from_count(app->inbe.maxbreaths), app->inbe.pause_seconds);
                apply_settings(&app->settings_preview, speed, app->inbe.max_rounds,
                               int_from_count(app->inbe.maxbreaths), app->inbe.pause_seconds);
                app->settings_dirty = 1;
            }
        } else if(app->tutorial_step == 3) {
            const char *lines[] = {
                "After the breathing round,",
                "exhale normally and hold.",
                "",
                "Release when your body asks",
                "for air. Do not force it."
            };
            draw_text_lines(lines, 5, x, &y, body_font, 20);
        } else {
            const char *lines[] = {
                "Inhale fully and hold for",
                "about 15 seconds.",
                "",
                "Then exhale and begin the",
                "next round. Over time, each",
                "round may feel deeper."
            };
            draw_tutorial_image(app->begin_image, "begin.png", 18, y, view_width - 36, 250);
            y += 272;
            draw_text_lines(lines, 6, x, &y, body_font, 20);
        }
    EndScissorMode();

    draw_scrollbar(app, &app->manual_scroll, content_h, viewport_h);
    draw_tab_bar(app);
    snprintf(page_label, sizeof(page_label), "%d/%d", app->tutorial_step + 1, TUTORIAL_STEPS);
    DrawText(page_label,
             view_width / 2 - MeasureText(page_label, 14) / 2,
             footer_mid_y, 14, c_text);

    int left_hover = 0;
    int right_hover = 0;
    if(app->tutorial_step == 0) {
        if(drawbtn(app, 50, footer_y, "SKIP", &left_hover))
            tutorial_close(app, 1);
    } else {
        if(drawbtn(app, 50, footer_y, "BACK", &left_hover)) {
            app->tutorial_step--;
            app->manual_scroll = 0;
        }
    }

    if(drawbtn(app, view_width - 52, footer_y,
               app->tutorial_step == TUTORIAL_STEPS - 1 ? "FINISH" : "NEXT", &right_hover)) {
        if(app->tutorial_step == TUTORIAL_STEPS - 1)
            tutorial_close(app, 1);
        else {
            app->tutorial_step++;
            app->manual_scroll = 0;
        }
    }

    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        app->settings_drag_slider = 0;
        app->settings_drag_scrollbar = 0;
        if(app->settings_dirty)
            save_settings(app);
    }
}

static void
draw_history(InbeApp *app)
{
    HistoryEntry entries[HISTORY_MAX_SESSIONS];
    int count = 0;
    int viewport_h = view_height - SETTINGS_TITLE_H - TAB_BAR_H;
    int row_h = 28;
    int content_rows = 0;
    int content_h = 0;
    int max_scroll;
    int close_hover = 0;
    int y;
    int has_year = 0;
    int has_month = 0;
    int has_day = 0;
    int selected_index = -1;

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
    app->history_scroll = clampi(app->history_scroll, 0, max_scroll);

    DrawRectangle(0, 0, view_width, SETTINGS_TITLE_H, darken(c_bg, 14));
    DrawLine(0, SETTINGS_TITLE_H - 1, view_width, SETTINGS_TITLE_H - 1, darken(c_bg, 42));
    DrawText("History", 12, 11, 18, c_text);

    if(drawiconbtn(app, view_width - 34, 7, 16, app->x_icon, &close_hover)) {
        app->inbe.screen = InbeScreenStart;
        app->history_scroll = 0;
    }

    BeginScissorMode((int)app->camera.offset.x,
                     (int)(app->camera.offset.y + SETTINGS_TITLE_H * app->camera.zoom),
                     (int)(view_width * app->camera.zoom),
                     (int)(viewport_h * app->camera.zoom));
        y = SETTINGS_TITLE_H + 12 - app->history_scroll;
        if(count == 0) {
            DrawText("No saved sessions yet.", 14, y, 14, c_text);
            DrawText("Complete a session to add data.", 14, y + 22, 14, c_text);
        } else {
            int year = -1;
            int month = -1;
            int day = -1;

            for(int i = 0; i < count; i++) {
                char label[HISTORY_TEXT_SIZE];

                if(entries[i].year != year) {
                    int selected = app->history_year == entries[i].year && app->history_level >= 1;

                    snprintf(label, sizeof(label), "%04d", entries[i].year);
                    if(draw_history_row(app, 12, y, view_width - 24, row_h, label, selected, 10)) {
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
                    if(draw_history_row(app, 12, y, view_width - 24, row_h, label, selected, 22)) {
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
                    if(draw_history_row(app, 12, y, view_width - 24, row_h, label, selected, 34)) {
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

                    snprintf(record_name, sizeof(record_name), "inbe-%02d%02d%02d",
                             entries[i].hour, entries[i].minute, entries[i].second);
                    history_format_session_label(&entries[i], time_label, sizeof(time_label));
                    selected = strcmp(app->history_record, record_name) == 0;
                    if(draw_history_row(app, 12, y, view_width - 24, row_h, time_label, selected, 46)) {
                        history_set_selected_record(app, &entries[i]);
                        app->history_level = 3;
                        selected_index = i;
                    }
                    y += row_h;

                    if(selected) {
                        for(int r = 0; r < entries[i].round_count; r++) {
                            char round_label[HISTORY_TEXT_SIZE];
                            history_format_round_label(&entries[i], r, round_label, sizeof(round_label));
                            DrawRectangle(12, y, view_width - 24, row_h, darken(c_bg, 4));
                            draw_bevel(12, y, view_width - 24, row_h, darken(c_bg, 24), lighten(c_bg, 14));
                            DrawText(round_label, 58, y + 6, 14, c_text);
                            y += row_h;
                        }
                    }
                }
            }
        }
    EndScissorMode();

    draw_scrollbar(app, &app->history_scroll, content_h, viewport_h);
    draw_tab_bar(app);

    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        app->settings_drag_scrollbar = 0;
}

void
inbe_app_init(void *vapp) {
    InbeApp *app = vapp;
    if(app == 0)
        return;

    load_config();
    inbeinit(&app->inbe);
    app->inbe.rmax = view_width * 0.4f;
    app->inbe.rmin = view_width * 0.2f;
    app->inbe.r = app->inbe.rmin;
    load_settings(app);
    prepare_history_storage();
    app->camera = (Camera2D){0};
    app->cursor_clickable = 0;
    app->settings_scroll = 0;
    app->settings_drag_slider = 0;
    app->settings_drag_scrollbar = 0;
    app->settings_dirty = 0;
    app->manual_scroll = 0;
    app->tutorial_step = 0;
    app->history_scroll = 0;
    app->history_level = 0;
    app->history_year = 0;
    app->history_month = 0;
    app->history_day = 0;
    app->history_record[0] = 0;
    app->session_paused = 0;
    app->results_saved = 0;
    reset_settings_preview(app);

    if(app->gear_icon.id == 0) {
        app->gear_icon = load_icon_texture("gear.png");
    }
    if(app->x_icon.id == 0) {
        app->x_icon = load_icon_texture("x.png");
    }
    if(app->manual_icon.id == 0) {
        app->manual_icon = load_icon_texture("manual.png");
    }
    if(app->return_icon.id == 0) {
        app->return_icon = load_icon_texture("return.png");
    }
    if(app->backward_icon.id == 0) {
        app->backward_icon = load_icon_texture("backward.png");
    }
    if(app->forward_icon.id == 0) {
        app->forward_icon = load_icon_texture("forward.png");
    }
    if(app->play_icon.id == 0) {
        app->play_icon = load_icon_texture("play.png");
    }
    if(app->pause_icon.id == 0) {
        app->pause_icon = load_icon_texture("pause.png");
    }
    if(app->stat_icon.id == 0) {
        app->stat_icon = load_icon_texture("stat.png");
    }
    if(app->home_icon.id == 0) {
        app->home_icon = load_icon_texture("home.png");
    }
    if(app->trash_icon.id == 0) {
        app->trash_icon = load_icon_texture("trash.png");
    }
    if(app->angel_image.id == 0) {
        app->angel_image = load_asset_texture("angel.png");
    }
    if(app->begin_image.id == 0) {
        app->begin_image = load_asset_texture("begin.png");
    }

    if(!app->tutorial_seen)
        app->inbe.screen = InbeScreenManual;
}

static void
updateapp(InbeApp *app)
{
    int center_x = view_width / 2;
    int center_y = view_height / 2;
    int hover = 0;

    if(app->inbe.screen == InbeScreenSettings) {
        draw_settings(app);
        app->inbe.frame++;
        return;
    }

    if(app->inbe.screen == InbeScreenManual) {
        draw_manual(app);
        app->inbe.frame++;
        return;
    }

    if(app->inbe.screen == InbeScreenHistory) {
        draw_history(app);
        app->inbe.frame++;
        return;
    }

    if(app->inbe.screen != InbeScreenResults)
        drawinbe(app, center_x, center_y);
    int title_font = 30;
    int title_w = 30;


    switch (app->inbe.screen) {
    case InbeScreenStart:
        title_w = MeasureText(config.title, title_font);
        DrawText(config.title, center_x - title_w / 2, 20, title_font, c_text);

        if (drawbtn(app, center_x, center_y + (int)app->inbe.rmin + 20, "PLAY", &hover)) {
            start_session(app);
        }
        draw_tab_bar(app);
        break;

    case InbeScreenSession:
        if(IsKeyPressed(KEY_BACKSPACE)) {
            inbe_app_init(app);
            break;
        }

        int return_hover = 0;
        if(app->return_icon.id != 0 && drawiconbtn(app, 10, 10, 16, app->return_icon, &return_hover)) {
            inbe_app_init(app);
            break;
        }

        int back_hover = 0;
        int pause_hover = 0;
        int forward_hover = 0;
        int control_y = view_height - 40;
        int control_size = 16;
        int control_gap = 8;
        int pause_x = center_x - (control_size + 8) / 2;
        int back_x = pause_x - control_size - 8 - control_gap;
        int forward_x = pause_x + control_size + 8 + control_gap;

        if(app->backward_icon.id != 0 && drawiconbtn(app, back_x, control_y, control_size,
                                                     app->backward_icon, &back_hover)) {
            session_step_back(app);
        }
        if(drawiconbtn(app, pause_x, control_y, control_size,
                       app->session_paused ? app->play_icon : app->pause_icon, &pause_hover)) {
            app->session_paused = !app->session_paused;
        }
        if(app->forward_icon.id != 0 && drawiconbtn(app, forward_x, control_y, control_size,
                                                    app->forward_icon, &forward_hover)) {
            session_step_forward(app);
        }

        draw_session_status(app, center_x, center_y);

        if(!app->session_paused)
            inbestep(&app->inbe);

        if (app->inbe.phase == InbePhaseHold) {
            int breath_y = center_y + app->inbe.rmin + 24;
            int breath_max_y = control_y - 44;
            if(breath_y > breath_max_y)
                breath_y = breath_max_y;
            if (drawbtn(app, center_x, breath_y, "BREATH", &hover)) {
                finish_hold(app);
            }
        }
        if(app->inbe.screen == InbeScreenResults)
            save_session_results(app);
        break;

    case InbeScreenResults:
        {
            int box_x = 12;
            int box_y = 78;
            int box_w = view_width - 24;
            int row_y = 180;
            int row_h = 26;
            int total = 0;
            int best = -1;
            int rounds = app->inbe.round + 1;

            title_w = MeasureText("RESULTS", title_font);
            DrawText("RESULTS", center_x - title_w / 2, 34, title_font, c_text);

            if(rounds < 1)
                rounds = 1;
            if(rounds > app->inbe.max_rounds)
                rounds = app->inbe.max_rounds;

            for(int i = 0; i < rounds; i++) {
                int seconds = int_from_count(app->inbe.results[i]);
                total += seconds;
                if(seconds > 0 && (best < 0 || seconds < best))
                    best = seconds;
            }

            if(best < 0)
                best = 0;

            DrawRectangle(box_x, box_y, box_w, 78, darken(c_bg, 6));
            DrawLine(box_x, box_y + 26, box_x + box_w, box_y + 26, darken(c_bg, 30));
            DrawLine(box_x, box_y + 52, box_x + box_w, box_y + 52, darken(c_bg, 30));
            DrawText(TextFormat("%d rounds", rounds), box_x + 10, box_y + 8, 16, c_text);
            DrawText(TextFormat("best %ds", best), box_x + 10, box_y + 34, 16, c_text);
            DrawText(TextFormat("avg %ds", rounds > 0 ? total / rounds : 0), box_x + 10, box_y + 60, 16, c_text);

            DrawText("Round times", box_x, 168, 14, darken(c_text, 20));
            for(int i = 0; i < rounds; i++) {
                char row[48];
                int seconds = int_from_count(app->inbe.results[i]);
                snprintf(row, sizeof(row), "Round %d  %ds", i + 1, seconds);
                DrawRectangle(box_x, row_y - 1, box_w, row_h, darken(c_bg, 4));
                DrawLine(box_x, row_y + row_h - 2, box_x + box_w, row_y + row_h - 2, darken(c_bg, 26));
                DrawText(row, box_x + 10, row_y + 5, 14, c_text);
                row_y += row_h;
            }

            if (drawbtn(app, center_x, view_height - 40, "HOME", &hover)) {
                inbe_app_init(app);
            }

            if(!app->results_saved)
                save_session_results(app);
        }
        break;

    }

    app->inbe.frame++;
}

void
inbe_app_update_draw(void *vapp, Rectangle viewport) {
    InbeApp *app = vapp;
    if(app == 0 || viewport.width <= 0 || viewport.height <= 0)
        return;

    refresh_theme_colors();

    float scale_x = viewport.width / (float)config.width;
    float scale_y = viewport.height / (float)config.height;
    float scale = scale_x;

    view_width = config.width;
    view_height = (int)(viewport.height / scale + 0.5f);
    if(view_height < config.height) {
        scale = scale_y;
        view_height = config.height;
        view_width = (int)(viewport.width / scale + 0.5f);
    }
    if(view_width < config.width)
        view_width = config.width;
    if(view_height < config.height)
        view_height = config.height;

    app->cursor_clickable = 0;
    app->camera.zoom = scale;
    app->camera.offset.x = viewport.x;
    app->camera.offset.y = viewport.y;

    BeginScissorMode((int)viewport.x, (int)viewport.y, (int)viewport.width, (int)viewport.height);
        DrawRectangleRec(viewport, c_bg);
        BeginMode2D(app->camera);
            DrawRectangle(0, 0, view_width, view_height, c_bg);
            updateapp(app);
        EndMode2D();
    EndScissorMode();
}

static void *
inbe_app_create(void)
{
    InbeApp *app = calloc(1, sizeof(InbeApp));
    inbe_app_init(app);
    return app;
}

static void
inbe_app_destroy(void *vapp)
{
    InbeApp *app = vapp;
    if(app != NULL) {
        if(app->gear_icon.id != 0)
            UnloadTexture(app->gear_icon);
        if(app->x_icon.id != 0)
            UnloadTexture(app->x_icon);
        if(app->manual_icon.id != 0)
            UnloadTexture(app->manual_icon);
        if(app->return_icon.id != 0)
            UnloadTexture(app->return_icon);
        if(app->backward_icon.id != 0)
            UnloadTexture(app->backward_icon);
        if(app->forward_icon.id != 0)
            UnloadTexture(app->forward_icon);
        if(app->play_icon.id != 0)
            UnloadTexture(app->play_icon);
        if(app->pause_icon.id != 0)
            UnloadTexture(app->pause_icon);
        if(app->stat_icon.id != 0)
            UnloadTexture(app->stat_icon);
        if(app->home_icon.id != 0)
            UnloadTexture(app->home_icon);
        if(app->trash_icon.id != 0)
            UnloadTexture(app->trash_icon);
        if(app->angel_image.id != 0)
            UnloadTexture(app->angel_image);
        if(app->begin_image.id != 0)
            UnloadTexture(app->begin_image);
        free(app);
    }
}

const LotusAppApi *
inbe_app_api(void)
{
    static const LotusAppApi api = {
        .id = "inbe",
        .create = inbe_app_create,
        .init = inbe_app_init,
        .update_draw = inbe_app_update_draw,
        .destroy = inbe_app_destroy
    };

    return &api;
}
