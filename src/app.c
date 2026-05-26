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

#if defined(PLATFORM_WEB)
#include <emscripten.h>
#endif

#define INBE_DEFAULT_TITLE "Inner Breeze"
#define INBE_DEFAULT_WIDTH 320
#define INBE_DEFAULT_HEIGHT 560
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
static float dpi_scale = 1.0f;
static Color c_text, c_bg, c_circle, c_button, c_button_hover, c_icon;

enum {
    SETTINGS_SPEED_MIN = 1,
    SETTINGS_SPEED_MAX = 16,
    SETTINGS_BREATHS_MIN = 15,
    SETTINGS_BREATHS_MAX = 80,
    SETTINGS_PAUSE_MIN = 0,
    SETTINGS_PAUSE_MAX = 30,
    SETTINGS_VOLUME_MIN = 0,
    SETTINGS_VOLUME_MAX = 100,
    SETTINGS_TITLE_H = 38,
    TAB_BAR_H = 48,
    SETTINGS_CONTENT_H = 526,
    CONTENT_MAX_W = 440,
    CONTENT_SIDE_PAD = 16,
    CIRCLE_SIDE_PAD = 24,
    TUTORIAL_STEPS = 5,
    HISTORY_MAX_SESSIONS = 48,
    HISTORY_PATH_SIZE = 96,
    HISTORY_TEXT_SIZE = 96,
    FS_PATH_MAX = 256
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

#if defined(PLATFORM_WEB)
#include <emscripten.h>

static int web_storage_ready = 0;

static void
init_web_storage(void)
{
    if(web_storage_ready)
        return;
    EM_ASM({
        try {
            FS.mkdir('/home');
            FS.mount(IDBFS, {root: '/'}, '/home');
            FS.syncfs(true, function(err) {
                if(err) console.error('IDBFS init sync failed:', err);
                else console.log('IDBFS initialized');
            });
        } catch(e) {
            console.error('IDBFS mount failed:', e);
        }
    });
    web_storage_ready = 1;
}

static void
sync_web_storage(void)
{
    EM_ASM({
        if(typeof FS !== 'undefined' && typeof FS.syncfs === 'function') {
            try {
                FS.syncfs(false, function(err) {
                    if(err) console.error('IDBFS save failed:', err);
                    else console.log('IDBFS synced');
                });
            } catch(e) {
                console.error('IDBFS sync error:', e);
            }
        }
    });
}
#endif

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

static int
ui_px(int px)
{
    return (int)(px * dpi_scale + 0.5f);
}

static int
ui_clamp_px(int px, int min_px, int max_px)
{
    int value = (int)(px * dpi_scale + 0.5f);
    int min_value = (int)(min_px * dpi_scale + 0.5f);
    int max_value = (int)(max_px * dpi_scale + 0.5f);

    if(value < min_value)
        value = min_value;
    if(value > max_value)
        value = max_value;
    return value;
}

static void
centered_column(int max_w, int side_pad, int *x, int *w)
{
    /* Scale parameters by DPI */
    max_w = (int)(max_w * dpi_scale + 0.5f);
    side_pad = (int)(side_pad * dpi_scale + 0.5f);

    int available_w = view_width - side_pad * 2;

    if(available_w < 0)
        available_w = 0;
    if(max_w > available_w)
        max_w = available_w;
    if(max_w < 0)
        max_w = 0;

    if(x != NULL)
        *x = (view_width - max_w) / 2;
    if(w != NULL)
        *w = max_w;
}

static int
clampi(int value, int min, int max);

static void
set_circle_bounds(Inbe *inbe, int rmin, int rmax)
{
    int old_rmin;
    int old_rmax;
    int old_span;
    int new_span;

    if(inbe == NULL)
        return;

    if(rmin < 8)
        rmin = 8;
    if(rmax < rmin + 8)
        rmax = rmin + 8;

    old_rmin = inbe->rmin;
    old_rmax = inbe->rmax;
    old_span = old_rmax - old_rmin;
    new_span = rmax - rmin;

    if(old_span > 0) {
        float t = (float)(inbe->r - old_rmin) / (float)old_span;

        if(t < 0.0f)
            t = 0.0f;
        if(t > 1.0f)
            t = 1.0f;

        inbe->r = rmin + t * (float)new_span;
    } else {
        inbe->r = rmin;
    }

    inbe->rmin = rmin;
    inbe->rmax = rmax;
    if(inbe->r < rmin)
        inbe->r = rmin;
    if(inbe->r > rmax)
        inbe->r = rmax;
}

static void
update_circle_bounds_for_view(Inbe *inbe, int top_reserve, int bottom_reserve)
{
    int available_w = view_width - CIRCLE_SIDE_PAD * 2;
    int available_h = view_height - top_reserve - bottom_reserve - CIRCLE_SIDE_PAD * 2;
    int rmax;
    int rmin;

    if(available_w < 0)
        available_w = 0;
    if(available_h < 0)
        available_h = 0;

    rmax = available_w / 2;
    if(available_h / 2 < rmax)
        rmax = available_h / 2;
    if(rmax < 24)
        rmax = 24;

    rmin = rmax / 2;
    if(rmin < 16)
        rmin = 16;
    if(rmin > rmax - 8)
        rmin = rmax - 8;

    set_circle_bounds(inbe, rmin, rmax);
}

static void
update_preview_bounds(Inbe *inbe, int content_w, int content_h)
{
    int span = content_w;
    int rmax;
    int rmin;

    if(content_h > 0 && content_h < span)
        span = content_h;

    rmax = span / 2;
    if(rmax < ui_px(60))
        rmax = ui_px(60);
    if(rmax > ui_px(120))
        rmax = ui_px(120);
    rmin = rmax / 2;
    if(rmin < ui_px(24))
        rmin = ui_px(24);
    if(rmin > rmax - ui_px(10))
        rmin = rmax - ui_px(10);

    set_circle_bounds(inbe, rmin, rmax);
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
    static const int breath_half_ticks[] = {108, 100, 93, 86, 80, 74, 68, 63, 58, 53, 48, 42, 36, 30, 24, 18};

    speed = clampi(speed, SETTINGS_SPEED_MIN, SETTINGS_SPEED_MAX);
    inbe->speed_level = speed;
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
    int content_w;

    inbeinit(&app->settings_preview);
    centered_column(CONTENT_MAX_W, CONTENT_SIDE_PAD, NULL, &content_w);
    update_preview_bounds(&app->settings_preview, content_w, ui_px(132));
    apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
    reset_round_breathe(&app->settings_preview);
}

static void
save_settings(InbeApp *app)
{
    char text[192];
    const char *settings_path =
#if defined(PLATFORM_WEB)
        "/home/settings.ini";
#else
        "settings.ini";
#endif
    snprintf(text, sizeof(text),
             "speed %d\nmax_rounds %d\nmax_breaths %d\npause_seconds %d\nsound_volume %d\ntutorial_seen %d\n",
             app->inbe.speed_level,
             app->inbe.max_rounds,
             int_from_count(app->inbe.maxbreaths),
             app->inbe.pause_seconds,
             app->sound_volume,
             app->tutorial_seen ? 1 : 0);
    SaveFileText(settings_path, text);
#if defined(PLATFORM_WEB)
    sync_web_storage();
#endif
    app->settings_dirty = 0;
}

static void
load_settings(InbeApp *app)
{
    const char *settings_path =
#if defined(PLATFORM_WEB)
        "/home/settings.ini";
#else
        "settings.ini";
#endif
    rini_data settings = rini_load(settings_path);

    int speed = rini_get_value_fallback(settings, "speed", 6);
    int max_rounds = rini_get_value_fallback(settings, "max_rounds", DefaultMaxRounds);
    int max_breaths = rini_get_value_fallback(settings, "max_breaths", DefaultMaxBreaths);
    int pause_seconds = rini_get_value_fallback(settings, "pause_seconds", DefaultPauseSeconds);
    int sound_volume = rini_get_value_fallback(settings, "sound_volume", 100);

    app->tutorial_seen = rini_get_value_fallback(settings, "tutorial_seen", 0) != 0;
    app->sound_volume = clampi(sound_volume, SETTINGS_VOLUME_MIN, SETTINGS_VOLUME_MAX);
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

static Sound
load_sound_asset(const char *name)
{
    char path[96];

    snprintf(path, sizeof(path), "assets/sounds/%s", name);
#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID) || defined(PLATFORM_WEB)
    return LoadSound(path);
#else
    if(FileExists(path))
        return LoadSound(path);

    snprintf(path, sizeof(path), "../inbe/assets/sounds/%s", name);
    if(FileExists(path))
        return LoadSound(path);

    snprintf(path, sizeof(path), "../assets/sounds/%s", name);
    return LoadSound(path);
#endif
}

static void
init_audio(InbeApp *app)
{
    if(app == NULL || app->audio_ready)
        return;

    InitAudioDevice();
    app->audio_ready = IsAudioDeviceReady();
    if(!app->audio_ready)
        return;

    app->breath_in_sound = load_sound_asset("breath-in.ogg");
    app->breath_out_sound = load_sound_asset("breath-out.ogg");
    app->bell_sound = load_sound_asset("bell.ogg");
}

static void
remember_sound_state(InbeApp *app)
{
    if(app == NULL)
        return;

    app->sound_last_screen = app->inbe.screen;
    app->sound_last_phase = app->inbe.phase;
    app->sound_last_dir = app->inbe.dir;
    cpcount(app->sound_last_count, app->inbe.count);
}

static void
play_app_sound(InbeApp *app, Sound sound, float scale)
{
    float volume;

    if(app == NULL || !app->audio_ready || sound.frameCount == 0 || app->sound_volume <= 0)
        return;

    volume = ((float)app->sound_volume / 100.0f) * scale;
    if(volume < 0.0f)
        volume = 0.0f;
    if(volume > 1.0f)
        volume = 1.0f;

    StopSound(sound);
    SetSoundVolume(sound, volume);
    PlaySound(sound);
}

static void
update_session_sounds(InbeApp *app)
{
    if(app == NULL)
        return;

    if(app->inbe.screen == InbeScreenSession && !app->session_paused) {
        if(app->sound_last_screen != InbeScreenSession) {
            if(app->inbe.phase == InbePhaseBreathe)
                play_app_sound(app, app->inbe.dir == 0 ? app->breath_in_sound : app->breath_out_sound, 1.0f);
        } else if(app->sound_last_phase != app->inbe.phase) {
            if(app->inbe.phase == InbePhaseBreathe)
                play_app_sound(app, app->inbe.dir == 0 ? app->breath_in_sound : app->breath_out_sound, 1.0f);
            else if(app->inbe.phase == InbePhaseHold)
                play_app_sound(app, app->bell_sound, 0.8f);
        } else if(app->inbe.phase == InbePhaseBreathe && app->sound_last_dir != app->inbe.dir) {
            play_app_sound(app, app->inbe.dir == 0 ? app->breath_in_sound : app->breath_out_sound, 1.0f);
        }
    }

    remember_sound_state(app);
}

static void
start_session(InbeApp *app)
{
    int speed = app->inbe.speed_level;
    int max_rounds = app->inbe.max_rounds;
    int max_breaths = int_from_count(app->inbe.maxbreaths);
    int pause_seconds = app->inbe.pause_seconds;

    inbeinit(&app->inbe);
    apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
    /* Save user's pause preference and use 3 seconds for first round */
    app->saved_pause_seconds = app->inbe.pause_seconds;
    app->inbe.pause_seconds = 3;
    update_circle_bounds_for_view(&app->inbe, 0, ui_clamp_px(TAB_BAR_H, 44, 56) + 80);
    app->inbe.screen = InbeScreenSession;
    app->session_paused = 0;
    app->results_saved = 0;
    remember_sound_state(app);
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
        /* Restore user's pause preference after round 0 */
        if(app->inbe.round == 1) {
            app->inbe.pause_seconds = app->saved_pause_seconds;
        }
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
    static char root[1024];

#if defined(PLATFORM_WEB)
    if(root[0] == '\0') {
        snprintf(root, sizeof(root), "/home/lotus/home");
        EM_ASM({
            try {
                FS.mkdir('/lotus');
                FS.mkdir('/lotus/home');
            } catch(e) {}
        });
    }
    return root;
#elif defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
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
    if(app->results_saved)
        return;

    time_t now;
    struct tm *tm;
    char dir_year[FS_PATH_MAX];
    char dir_month[FS_PATH_MAX];
    char dir_day[FS_PATH_MAX];
    char path[FS_PATH_MAX];
    char text[MaxRounds * 8];
    int offset = 0;
    int played_rounds;

    now = time(NULL);
    tm = localtime(&now);
    if(tm == NULL)
        return;

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
#if defined(PLATFORM_WEB)
        sync_web_storage();
#endif
    } else {
        TraceLog(LOG_WARNING, "INBE: failed to save results to %s", path);
    }
}

static void
prepare_history_storage(void)
{
    time_t now;
    struct tm *tm;
    char dir_year[FS_PATH_MAX];
    char dir_month[FS_PATH_MAX];
    char dir_day[FS_PATH_MAX];

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
    char child[FS_PATH_MAX];

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
    char ypath[FS_PATH_MAX];
    char mpath[FS_PATH_MAX];
    char dpath[FS_PATH_MAX];

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
                    // Set to day level (2) without selecting specific time
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

        // Default to day level for most recent entry, no specific time
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

    DrawText(text, x + ui_px(indent), y + ui_px(6), ui_clamp_px(14, 12, 16), c_text);
    return hover && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

static int
draw_history_session_row(InbeApp *app, int x, int y, int w, int h, const char *text, int selected)
{
    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
    int mx = (int)mouse_world.x;
    int my = (int)mouse_world.y;
    int hover = 0;
    int icon_size = ui_clamp_px(16, 14, 18);
    int font = ui_clamp_px(14, 12, 16);
    (void)font; /* Currently unused but may be needed for future */

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

    /* Draw text */
    DrawText(text, x + ui_px(46), y + ui_px(6), font, c_text);

    /* Draw trash icon on the right */
    if(app->trash_icon.id != 0) {
        int trash_hover = 0;
        int trash_x = x + w - icon_size - ui_px(8);
        int trash_y = y + (h - icon_size) / 2;
        Rectangle src = {0, 0, app->trash_icon.width, app->trash_icon.height};
        Rectangle dst = {trash_x, trash_y, (float)icon_size, (float)icon_size};

        /* Check if trash icon is hovered */
        if(mx > trash_x && mx < trash_x + icon_size && my > trash_y && my < trash_y + icon_size) {
            (void)trash_hover; /* Mark as intentionally unused for future hover effects */
            app->cursor_clickable = 1;
            DrawTexturePro(app->trash_icon, src, dst, (Vector2){0}, 0, darken(c_icon, 30));
            if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
                return 2; /* Return 2 to indicate trash clicked */
        } else {
            DrawTexturePro(app->trash_icon, src, dst, (Vector2){0}, 0, c_icon);
        }
    }

    if(hover && IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        return 1; /* Return 1 to indicate row clicked */

    return 0;
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

    snprintf(out, (size_t)out_size, "%02d:%02d  avg %ds",
             entry->hour, entry->minute, entry->avg_seconds);
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
    int font = ui_clamp_px(20, 16, 22);
    int w = (int)MeasureText(label, font) + ui_px(20);
    int h = ui_clamp_px(30, 26, 34);

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

    DrawText(label, x + ui_px(10), y + ui_px(5), font, c_text);

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

static int
nav_button_width(const char *label, int icon_size, int show_label, int font)
{
    int width = icon_size + ui_px(8);

    if(show_label && label != NULL && label[0] != '\0')
        width += ui_px(10) + MeasureText(label, font);
    return width;
}

static int
draw_nav_button(InbeApp *app, int x, int y, int icon_size, Texture2D icon, const char *label,
                int show_label, int *hover)
{
    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
    int mx = (int)mouse_world.x;
    int my = (int)mouse_world.y;
    int mb = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    int released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    int font = ui_clamp_px(14, 12, 16);
    int w = nav_button_width(label, icon_size, show_label, font);
    int h = ui_clamp_px(30, 26, 34);
    int pressed = 0;

    if(mx > x && mx < x + w && my > y && my < y + h) {
        DrawRectangle(x, y, w, h, c_button_hover);
        draw_bevel(x, y, w, h, darken(c_button_hover, 40), lighten(c_button_hover, 40));
        *hover = 1;
        app->cursor_clickable = 1;
        if(mb)
            draw_bevel(x, y, w, h, lighten(c_button_hover, 40), darken(c_button_hover, 40));
        if(released)
            pressed = 1;
    } else {
        DrawRectangle(x, y, w, h, c_button);
        draw_bevel(x, y, w, h, lighten(c_button, 40), darken(c_button, 40));
        *hover = 0;
    }

    if(icon.id != 0) {
        Rectangle src = {0, 0, icon.width, icon.height};
        Rectangle dst = {x + ui_px(4), y + ui_px(4), (float)icon_size, (float)icon_size};
        DrawTexturePro(icon, src, dst, (Vector2){0}, 0, c_icon);
    }

    if(show_label && label != NULL && label[0] != '\0') {
        int text_x = x + icon_size + ui_px(12);
        int text_y = y + (h - font) / 2;
        DrawText(label, text_x, text_y, font, c_text);
    }

    return pressed;
}

static void
draw_tab_bar(InbeApp *app)
{
    int bar_h = ui_clamp_px(TAB_BAR_H, 44, 56);
    int bar_y = view_height - bar_h;
    int button_size = ui_clamp_px(20, 18, 24);
    int show_labels = view_width >= ui_px(420);
    int font = ui_clamp_px(14, 12, 16);
    int stat_w = nav_button_width("History", button_size, show_labels, font);
    int manual_w = nav_button_width("Manual", button_size, show_labels, font);
    int gear_w = nav_button_width("Settings", button_size, show_labels, font);
    int group_gap = ui_px(10);
    int side_margin = ui_px(16);
    int group_w = stat_w + manual_w + gear_w + group_gap * 2;
    int available_w = view_width - side_margin * 2;
    int group_x, button_y;

    /* Center the button group, but don't exceed margins */
    if(group_w > available_w)
        group_w = available_w;
    group_x = side_margin + (available_w - group_w) / 2;
    button_y = bar_y + (bar_h - ui_clamp_px(30, 26, 34)) / 2;
    int tab_hover = 0;

    DrawRectangle(0, bar_y, view_width, bar_h, darken(c_bg, 10));
    DrawLine(0, bar_y, view_width, bar_y, darken(c_bg, 42));

    if(app->stat_icon.id != 0) {
        if(draw_nav_button(app, group_x, button_y, button_size, app->stat_icon,
                           "History", show_labels, &tab_hover)) {
            history_open_latest(app);
            app->inbe.screen = InbeScreenHistory;
        }
    }
    if(app->manual_icon.id != 0) {
        if(draw_nav_button(app, group_x + stat_w + group_gap, button_y, button_size,
                           app->manual_icon, "Manual", show_labels, &tab_hover)) {
            app->tutorial_step = 0;
            app->inbe.screen = InbeScreenManual;
        }
    }
    if(app->gear_icon.id != 0) {
        if(draw_nav_button(app, group_x + stat_w + manual_w + group_gap * 2, button_y,
                           button_size, app->gear_icon, "Settings", show_labels, &tab_hover)) {
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
    char max_text[32];
    int total_seconds;
    int remaining;
    int max_text_w;
    int text_y;

    if(app->inbe.phase != InbePhaseStarting)
        return;

    if(app->inbe.round == 0) {
        total_seconds = 3;
    } else {
        if(app->inbe.pause_seconds <= 0)
            return;
        total_seconds = app->inbe.pause_seconds;
    }

    remaining = total_seconds - app->inbe.sectick / 60;
    if(remaining < 1)
        remaining = 1;

    int font = ui_clamp_px(18, 16, 20);
    /* Calculate fixed width based on maximum possible value */
    snprintf(max_text, sizeof(max_text), "STARTING IN %2d", 30);
    max_text_w = MeasureText(max_text, font);

    snprintf(text, sizeof(text), "STARTING IN %2d", remaining);
    text_y = center_y - (int)(app->inbe.rmax * 0.72f) - ui_px(40);
    if(text_y < ui_px(20))
        text_y = ui_px(20);
    DrawText(text, center_x - max_text_w / 2, text_y, font, c_text);
}

static void
draw_session_counter(InbeApp *app, int center_x, int center_y)
{
    char text[CountSize];
    int count;
    int text_w;
    int font = ui_clamp_px(20, 18, 24);
    int y_off = ui_px(10);

    if(app->inbe.phase == InbePhaseRecover) {
        if(app->inbe.r < app->inbe.rmax) {
            DrawText("000", center_x - MeasureText("000", font) / 2, center_y - y_off, font, c_text);
            return;
        }

        count = int_from_count(app->inbe.count);
        if(count < 15) {
            count_from_int(text, 15 - count);
            text_w = MeasureText(text, font);
            DrawText(text, center_x - text_w / 2, center_y - y_off, font, c_text);
            return;
        }
        DrawText("000", center_x - MeasureText("000", font) / 2, center_y - y_off, font, c_text);
        return;
    }

    if(app->inbe.phase == InbePhaseNext) {
        DrawText("000", center_x - MeasureText("000", font) / 2, center_y - y_off, font, c_text);
        return;
    }

    text_w = MeasureText(app->inbe.count, font);
    DrawText(app->inbe.count, center_x - text_w / 2, center_y - y_off, font, c_text);
}

static void
draw_preview_inbe(Inbe *inbe, int center_x, int center_y)
{
    int r = (int)((float)inbe->r * 0.72f);
    DrawCircle(center_x, center_y, r, c_circle);
    DrawCircleLines(center_x, center_y, r, c_text);
}

static int
draw_slider(InbeApp *app, int id, int x, int y, int w, const char *label,
            int min, int max, int *value, const char *suffix)
{
    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
    int mx = (int)mouse_world.x;
    int label_font = ui_clamp_px(16, 14, 18);
    int value_font = ui_clamp_px(16, 14, 18);
    int track_y = y + ui_px(28);
    int track_h = ui_px(8);
    int knob_w = ui_px(12);
    int knob_h = ui_px(22);
    int knob_y = track_y - (knob_h - track_h) / 2;
    int hit_padding = ui_px(16);
    int changed = 0;
    char value_text[32];
    Rectangle hit = {(float)(x - hit_padding), (float)(knob_y - hit_padding), (float)(w + hit_padding * 2), (float)(knob_h + hit_padding * 2)};

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
    DrawRectangle(knob_x, knob_y, knob_w, knob_h, c_button);
    draw_bevel(knob_x, knob_y, knob_w, knob_h, lighten(c_button, 40), darken(c_button, 40));

    return changed;
}

static void
draw_scrollbar(InbeApp *app, int *scroll, int content_h, int viewport_h)
{
    if(content_h <= viewport_h)
        return;

    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
    int title_h = ui_clamp_px(SETTINGS_TITLE_H, 34, 44);
    int bar_x = view_width - 6;
    int bar_y = title_h;
    int bar_w = 6;
    int bar_h = viewport_h;
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
    int title_h = ui_clamp_px(SETTINGS_TITLE_H, 34, 44);
    int tab_h = ui_clamp_px(TAB_BAR_H, 44, 56);
    int viewport_h = view_height - title_h - tab_h;
    int max_scroll = SETTINGS_CONTENT_H - viewport_h;
    int gear_hover = 0;
    int content_x;
    int content_w;

    if(max_scroll < 0)
        max_scroll = 0;

    centered_column(CONTENT_MAX_W, CONTENT_SIDE_PAD, &content_x, &content_w);

    app->settings_scroll -= (int)(GetMouseWheelMove() * 24.0f);
    app->settings_scroll = clampi(app->settings_scroll, 0, max_scroll);

    DrawRectangle(0, 0, view_width, title_h, darken(c_bg, 14));
    DrawLine(0, title_h - 1, view_width, title_h - 1, darken(c_bg, 42));
    DrawText("Settings", ui_px(12), ui_px(11), ui_clamp_px(18, 16, 20), c_text);

    if(drawiconbtn(app, view_width - ui_px(40), ui_px(8), ui_clamp_px(16, 14, 18), app->x_icon, &gear_hover)) {
        if(app->settings_dirty)
            save_settings(app);
        app->inbe.screen = InbeScreenStart;
        app->settings_scroll = 0;
    }

    BeginScissorMode((int)app->camera.offset.x,
                     (int)(app->camera.offset.y + title_h * app->camera.zoom),
                     (int)(view_width * app->camera.zoom),
                     (int)(viewport_h * app->camera.zoom));
        int yoff = title_h - app->settings_scroll;
        int speed = app->inbe.speed_level;
        int max_rounds = app->inbe.max_rounds;
        int max_breaths = int_from_count(app->inbe.maxbreaths);
        int pause_seconds = app->inbe.pause_seconds;
        int sound_volume = app->sound_volume;

        update_preview_bounds(&app->settings_preview, content_w, ui_px(240));
        apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
        inbestep(&app->settings_preview);
        if(app->settings_preview.phase != InbePhaseBreathe) {
            reset_settings_preview(app);
            update_preview_bounds(&app->settings_preview, content_w, ui_px(240));
            apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
        }

        draw_preview_inbe(&app->settings_preview, content_x + content_w / 2, yoff + ui_px(96));

        if(draw_slider(app, 1, content_x, yoff + ui_px(212), content_w, "Speed", SETTINGS_SPEED_MIN,
                       SETTINGS_SPEED_MAX, &speed, "")) {
            apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
            apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
            app->settings_dirty = 1;
        }

        if(draw_slider(app, 2, content_x, yoff + ui_px(278), content_w, "Max rounds", 1,
                       MaxRounds, &max_rounds, "")) {
            apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
            apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
            app->settings_dirty = 1;
        }

        if(draw_slider(app, 3, content_x, yoff + ui_px(344), content_w, "Max breaths", SETTINGS_BREATHS_MIN,
                       SETTINGS_BREATHS_MAX, &max_breaths, "")) {
            apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
            apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
            app->settings_dirty = 1;
        }

        if(draw_slider(app, 4, content_x, yoff + ui_px(410), content_w, "Pause after round", SETTINGS_PAUSE_MIN,
                       SETTINGS_PAUSE_MAX, &pause_seconds, "s")) {
            apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
            apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
            app->settings_dirty = 1;
        }

        if(draw_slider(app, 5, content_x, yoff + ui_px(476), content_w, "Volume", SETTINGS_VOLUME_MIN,
                       SETTINGS_VOLUME_MAX, &sound_volume, "")) {
            app->sound_volume = sound_volume;
            app->settings_dirty = 1;
        }
    EndScissorMode();

    draw_scrollbar(app, &app->settings_scroll, SETTINGS_CONTENT_H, viewport_h);

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
    int font = ui_clamp_px(14, 12, 16);
    int tw = MeasureText(label, font);
    DrawText(label, x + w / 2 - tw / 2, y + h / 2 - ui_px(7), font, c_text);
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
    int title_h = ui_clamp_px(SETTINGS_TITLE_H, 34, 44);
    int tab_h = ui_clamp_px(TAB_BAR_H, 44, 56);
    int viewport_h = view_height - title_h - tab_h;
    int content_h = 430;
    int title_font = ui_clamp_px(18, 16, 20);
    int body_font = ui_clamp_px(14, 12, 16);
    int title_w;
    int previous_step;
    int max_scroll;
    int content_x;
    int content_w;
    const char *title = "Tutorial";
    int footer_y = view_height - ui_px(38);
    int footer_mid_y = footer_y + ui_px(7);
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

    max_scroll = content_h - viewport_h;
    if(max_scroll < 0)
        max_scroll = 0;
    app->manual_scroll -= (int)(GetMouseWheelMove() * 24.0f);
    app->manual_scroll = clampi(app->manual_scroll, 0, max_scroll);

    centered_column(CONTENT_MAX_W, CONTENT_SIDE_PAD, &content_x, &content_w);

    DrawRectangle(0, 0, view_width, title_h, darken(c_bg, 14));
    DrawLine(0, title_h - 1, view_width, title_h - 1, darken(c_bg, 42));
    title_w = MeasureText(title, title_font);
    DrawText(title, view_width / 2 - title_w / 2, ui_px(11), title_font, c_text);

    int x_hover = 0;
    if(drawiconbtn(app, view_width - ui_px(34), ui_px(7), ui_clamp_px(16, 14, 18), app->x_icon, &x_hover))
        tutorial_close(app, 1);

    BeginScissorMode((int)app->camera.offset.x,
                     (int)(app->camera.offset.y + title_h * app->camera.zoom),
                     (int)(view_width * app->camera.zoom),
                     (int)(viewport_h * app->camera.zoom));
        int y = title_h + ui_px(16) - app->manual_scroll;
        if(app->tutorial_step == 0) {
            const char *lines[] = {
                "This breathing practice can be",
                "powerful. Use it with care.",
                "",
                "Practice sitting or lying down.",
                "Never use it while driving,",
                "standing, or in water."
            };
            int img_h = ui_px(240);
            draw_tutorial_image(app->angel_image, "angel.jpg", content_x, y, content_w, img_h);
            y += img_h + ui_px(22);
            draw_text_lines(lines, 6, content_x, &y, body_font, ui_px(20));
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
            draw_text_lines(lines, 10, content_x, &y, body_font, ui_px(19));
            if(app->gear_icon.id != 0) {
                int gear_hover = 0;
                int gear_font = ui_clamp_px(14, 12, 16);
                DrawText("Settings", content_x, y + ui_px(7), gear_font, c_text);
                drawiconbtn(app, content_x + ui_px(80), y, ui_clamp_px(16, 14, 18), app->gear_icon, &gear_hover);
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
            draw_text_lines(lines, 5, content_x, &y, body_font, ui_px(19));
            y += ui_px(8);

            update_preview_bounds(&app->settings_preview, content_w, ui_px(132));
            apply_settings(&app->settings_preview, speed, app->inbe.max_rounds,
                           int_from_count(app->inbe.maxbreaths), app->inbe.pause_seconds);
            inbestep(&app->settings_preview);
            if(app->settings_preview.phase != InbePhaseBreathe) {
                reset_settings_preview(app);
                update_preview_bounds(&app->settings_preview, content_w, ui_px(132));
                apply_settings(&app->settings_preview, speed, app->inbe.max_rounds,
                               int_from_count(app->inbe.maxbreaths), app->inbe.pause_seconds);
            }
            draw_preview_inbe(&app->settings_preview, content_x + content_w / 2, y + ui_px(40));
            y += (int)(app->settings_preview.rmax * 0.72f) + ui_px(54);

            if(draw_slider(app, 10, content_x, y, content_w, "Speed", SETTINGS_SPEED_MIN,
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
            draw_text_lines(lines, 5, content_x, &y, body_font, ui_px(20));
        } else {
            const char *lines[] = {
                "Inhale fully and hold for",
                "about 15 seconds.",
                "",
                "Then exhale and begin the",
                "next round. Over time, each",
                "round may feel deeper."
            };
            int img_h = ui_px(250);
            draw_tutorial_image(app->begin_image, "begin.jpg", content_x, y, content_w, img_h);
            y += img_h + ui_px(22);
            draw_text_lines(lines, 6, content_x, &y, body_font, ui_px(20));
        }
    EndScissorMode();

    draw_scrollbar(app, &app->manual_scroll, content_h, viewport_h);
    snprintf(page_label, sizeof(page_label), "%d/%d", app->tutorial_step + 1, TUTORIAL_STEPS);
    DrawText(page_label,
             view_width / 2 - MeasureText(page_label, ui_clamp_px(14, 14, 16)) / 2,
             footer_mid_y, ui_clamp_px(14, 14, 16), c_text);

    int left_hover = 0;
    int right_hover = 0;
    int button_pad = ui_px(48);
    if(app->tutorial_step == 0) {
        if(drawbtn(app, content_x + button_pad, footer_y, "SKIP", &left_hover))
            tutorial_close(app, 1);
    } else {
        if(drawbtn(app, content_x + button_pad, footer_y, "BACK", &left_hover)) {
            app->tutorial_step--;
            app->manual_scroll = 0;
        }
    }

    if(drawbtn(app, content_x + content_w - button_pad, footer_y,
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
    int title_h = ui_clamp_px(SETTINGS_TITLE_H, 34, 44);
    int tab_h = ui_clamp_px(TAB_BAR_H, 44, 56);
    int viewport_h = view_height - title_h - tab_h;
    int row_h = ui_clamp_px(28, 24, 32);
    int content_rows = 0;
    int content_h = 0;
    int max_scroll;
    int close_hover = 0;
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
    app->history_scroll = clampi(app->history_scroll, 0, max_scroll);

    centered_column(CONTENT_MAX_W, CONTENT_SIDE_PAD, &content_x, &content_w);

    DrawRectangle(0, 0, view_width, title_h, darken(c_bg, 14));
    DrawLine(0, title_h - 1, view_width, title_h - 1, darken(c_bg, 42));
    DrawText("History", ui_px(12), ui_px(11), ui_clamp_px(18, 16, 20), c_text);

    if(drawiconbtn(app, view_width - ui_px(40), ui_px(8), ui_clamp_px(16, 14, 18), app->x_icon, &close_hover)) {
        app->inbe.screen = InbeScreenStart;
        app->history_scroll = 0;
    }

    BeginScissorMode((int)app->camera.offset.x,
                     (int)(app->camera.offset.y + title_h * app->camera.zoom),
                     (int)(view_width * app->camera.zoom),
                     (int)(viewport_h * app->camera.zoom));
        y = title_h + ui_px(12) - app->history_scroll;
        if(count == 0) {
            DrawText("No saved sessions yet.", content_x, y, ui_clamp_px(14, 12, 16), c_text);
            DrawText("Complete a session to add data.", content_x, y + ui_px(22), ui_clamp_px(14, 12, 16), c_text);
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
                        /* Row clicked - select session */
                        history_set_selected_record(app, &entries[i]);
                        app->history_level = 3;
                        selected_index = i;
                    } else if(result == 2) {
                        /* Trash clicked - delete session file */
                        char dir_day[FS_PATH_MAX];
                        char path[FS_PATH_MAX];
                        snprintf(dir_day, sizeof(dir_day), "%s/%04d/%02d/%02d",
                                 history_root(), entries[i].year, entries[i].month, entries[i].day);
                        snprintf(path, sizeof(path), "%s/%s", dir_day, record_name);
                        remove(path);
                        /* Reload history */
                        scan_history_tree(entries, &count);
                        qsort(entries, (size_t)count, sizeof(entries[0]), compare_history_entries);
                        history_clear_record_selection(app);
                        selected_index = -1;
                        /* Rebuild content rows */
                        content_rows = history_count_year_rows(entries, count);
                        if(count > 0 && has_year && app->history_level >= 1) {
                            content_rows += history_count_month_rows(entries, count, app->history_year);
                            if(has_month && app->history_level >= 2) {
                                content_rows += history_count_day_rows(entries, count, app->history_year, app->history_month);
                                if(has_day && app->history_level >= 3) {
                                    content_rows += history_count_record_rows(entries, count,
                                                                             app->history_year, app->history_month,
                                                                             app->history_day);
                                }
                            }
                        }
                        content_h = 18 + content_rows * row_h;
                        max_scroll = content_h - viewport_h;
                        if(max_scroll < 0)
                            max_scroll = 0;
                        app->history_scroll = clampi(app->history_scroll, 0, max_scroll);
                        y -= row_h;  /* Don't advance y since we removed this row */
                        continue;
                    }
                    y += row_h;

                    if(selected) {
                        for(int r = 0; r < entries[i].round_count; r++) {
                            char round_label[HISTORY_TEXT_SIZE];
                            history_format_round_label(&entries[i], r, round_label, sizeof(round_label));
                            DrawRectangle(content_x, y, content_w, row_h, darken(c_bg, 4));
                            draw_bevel(content_x, y, content_w, row_h, darken(c_bg, 24), lighten(c_bg, 14));
                            DrawText(round_label, content_x + ui_px(46), y + ui_px(6), ui_clamp_px(14, 12, 16), c_text);
                            y += row_h;
                        }
                    }
                }
            }
        }
    EndScissorMode();

    draw_scrollbar(app, &app->history_scroll, content_h, viewport_h);

    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        app->settings_drag_scrollbar = 0;
}

void
inbe_app_init(void *vapp) {
    InbeApp *app = vapp;
    if(app == 0)
        return;

#if defined(PLATFORM_WEB)
    init_web_storage();
#endif
    load_config();
    inbeinit(&app->inbe);
    update_circle_bounds_for_view(&app->inbe, ui_clamp_px(SETTINGS_TITLE_H, 34, 44),
                                  ui_clamp_px(TAB_BAR_H, 44, 56) + ui_px(80));
    load_settings(app);
    update_circle_bounds_for_view(&app->inbe, ui_clamp_px(SETTINGS_TITLE_H, 34, 44),
                                  ui_clamp_px(TAB_BAR_H, 44, 56) + 80);
    prepare_history_storage();
    init_audio(app);
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
    remember_sound_state(app);
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
        app->angel_image = load_asset_texture("angel.jpg");
    }
    if(app->begin_image.id == 0) {
        app->begin_image = load_asset_texture("begin.jpg");
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

    if(app->inbe.screen == InbeScreenStart) {
        update_circle_bounds_for_view(&app->inbe, ui_clamp_px(SETTINGS_TITLE_H, 34, 44),
                                      ui_clamp_px(TAB_BAR_H, 44, 56) + 96);
    } else if(app->inbe.screen == InbeScreenSession) {
        update_circle_bounds_for_view(&app->inbe, 0, 84);
    }

    if(app->inbe.screen != InbeScreenResults)
        drawinbe(app, center_x, center_y);
    int title_font = ui_clamp_px(30, 24, 34);
    int title_w = 30;


    switch (app->inbe.screen) {
    case InbeScreenStart:
        title_w = MeasureText(config.title, title_font);
        DrawText(config.title, center_x - title_w / 2, ui_px(20), title_font, c_text);

        {
            int play_y = center_y + (int)(app->inbe.rmax * dpi_scale + 0.5f) + ui_px(20);
            int play_limit = view_height - ui_clamp_px(TAB_BAR_H, 44, 56) - ui_px(48);
            if(play_y > play_limit)
                play_y = play_limit;
            if (drawbtn(app, center_x, play_y, "PLAY", &hover)) {
            start_session(app);
            }
        }
        draw_tab_bar(app);
        break;

    case InbeScreenSession:
        if(IsKeyPressed(KEY_BACKSPACE)) {
            inbe_app_init(app);
            break;
        }

        int return_hover = 0;
        if(app->return_icon.id != 0 && drawiconbtn(app, ui_px(12), ui_px(12), ui_clamp_px(24, 20, 28), app->return_icon, &return_hover)) {
            inbe_app_init(app);
            break;
        }

        int back_hover = 0;
        int pause_hover = 0;
        int forward_hover = 0;
        int control_y = view_height - ui_px(40);
        int control_size = ui_clamp_px(16, 14, 20);
        int control_gap = ui_px(8);
        int control_pad = ui_px(8);
        int pause_x = center_x - (control_size + control_pad) / 2;
        int back_x = pause_x - control_size - control_pad - control_gap;
        int forward_x = pause_x + control_size + control_pad + control_gap;

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
        update_session_sounds(app);

        if (app->inbe.phase == InbePhaseHold) {
            int breath_y = center_y + (int)(app->inbe.rmax * dpi_scale + 0.5f) + ui_px(24);
            int breath_max_y = control_y - ui_px(44);
            if(breath_y > breath_max_y)
                breath_y = breath_max_y;
            if (drawbtn(app, center_x, breath_y, "BREATH", &hover)) {
                finish_hold(app);
            }
        }
        break;

    case InbeScreenResults:
        {
            int box_x;
            int box_y = ui_px(78);
            int box_w;
            int row_y = ui_px(180);
            int row_h = ui_clamp_px(26, 22, 30);
            int total = 0;
            int best = -1;
            int rounds = app->inbe.round + 1;

            centered_column(CONTENT_MAX_W, CONTENT_SIDE_PAD, &box_x, &box_w);
            title_w = MeasureText("RESULTS", title_font);
            DrawText("RESULTS", center_x - title_w / 2, ui_px(34), title_font, c_text);

            if(rounds < 1)
                rounds = 1;
            if(rounds > app->inbe.max_rounds)
                rounds = app->inbe.max_rounds;

            for(int i = 0; i < rounds; i++) {
                int seconds = int_from_count(app->inbe.results[i]);
                total += seconds;
                if(seconds > 0 && (best < 0 || seconds > best))
                    best = seconds;
            }

            if(best < 0)
                best = 0;

            DrawRectangle(box_x, box_y, box_w, ui_px(78), darken(c_bg, 6));
            DrawLine(box_x, box_y + ui_px(26), box_x + box_w, box_y + ui_px(26), darken(c_bg, 30));
            DrawLine(box_x, box_y + ui_px(52), box_x + box_w, box_y + ui_px(52), darken(c_bg, 30));
            DrawText(TextFormat("%d rounds", rounds), box_x + ui_px(10), box_y + ui_px(8), ui_clamp_px(16, 14, 18), c_text);
            DrawText(TextFormat("best %ds", best), box_x + ui_px(10), box_y + ui_px(34), ui_clamp_px(16, 14, 18), c_text);
            DrawText(TextFormat("avg %ds", rounds > 0 ? total / rounds : 0), box_x + ui_px(10), box_y + ui_px(60), ui_clamp_px(16, 14, 18), c_text);

            DrawText("Round times", box_x, ui_px(168), ui_clamp_px(14, 12, 16), darken(c_text, 20));
            for(int i = 0; i < rounds; i++) {
                char row[48];
                int seconds = int_from_count(app->inbe.results[i]);
                snprintf(row, sizeof(row), "Round %d  %ds", i + 1, seconds);
                DrawRectangle(box_x, row_y - 1, box_w, row_h, darken(c_bg, 4));
                DrawLine(box_x, row_y + row_h - 2, box_x + box_w, row_y + row_h - 2, darken(c_bg, 26));
                DrawText(row, box_x + ui_px(10), row_y + ui_px(5), ui_clamp_px(14, 12, 16), c_text);
                row_y += row_h;
            }

            if (drawbtn(app, center_x, view_height - ui_px(40), "HOME", &hover)) {
                inbe_app_init(app);
            }
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

    view_width = (int)viewport.width;
    view_height = (int)viewport.height;

    /* Calculate DPI scale based on viewport height vs base design (560) */
    dpi_scale = (float)view_height / (float)INBE_DEFAULT_HEIGHT;
    if(dpi_scale < 1.0f)
        dpi_scale = 1.0f;

    app->cursor_clickable = 0;
    app->camera.zoom = 1.0f;
    app->camera.offset.x = viewport.x;
    app->camera.offset.y = viewport.y;

    DrawRectangleRec(viewport, c_bg);
    BeginScissorMode((int)viewport.x, (int)viewport.y, (int)viewport.width, (int)viewport.height);
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
        if(app->breath_in_sound.frameCount != 0)
            UnloadSound(app->breath_in_sound);
        if(app->breath_out_sound.frameCount != 0)
            UnloadSound(app->breath_out_sound);
        if(app->bell_sound.frameCount != 0)
            UnloadSound(app->bell_sound);
        if(app->audio_ready) {
            CloseAudioDevice();
            app->audio_ready = 0;
        }
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
