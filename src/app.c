#define RINI_IMPLEMENTATION
#include "app.h"
#include "theme.h"
#include "theme_meta.h"
#include "version.h"
#include "ui.h"
#include "../vendor/rini/src/rini.h"

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
    SETTINGS_TITLE_H = 50,
    TAB_BAR_H = 58,
    SETTINGS_CONTENT_H = 400,
    CONTENT_MAX_W = 440,
    CONTENT_SIDE_PAD = 16,
    CIRCLE_SIDE_PAD = 24,
    TUTORIAL_STEPS = 5,
    HISTORY_MAX_SESSIONS = 48,
    HISTORY_PATH_SIZE = 256,
    HISTORY_TEXT_SIZE = 96,
    FS_PATH_MAX = 512
};

enum {
    SETTINGS_TAB_BREATHING = 0,
    SETTINGS_TAB_SESSION = 1,
    SETTINGS_TAB_APPEARANCE = 2,
    SETTINGS_TAB_ABOUT = 3,
    SETTINGS_TAB_COUNT = 4
};

static const char *settings_tab_names[SETTINGS_TAB_COUNT] = {
    "Breathing",
    "Session",
    "Appearance",
    "About"
};

enum {
    /* Icon sizes (base values in logical pixels) */
    ICON_SIZE_SMALL = 22,
    ICON_SIZE_MEDIUM = 26,
    ICON_SIZE_LARGE = 30,
    /* Min/max ranges for DPI scaling */
    ICON_SIZE_SMALL_MIN = 20,
    ICON_SIZE_SMALL_MAX = 36,
    ICON_SIZE_MEDIUM_MIN = 24,
    ICON_SIZE_MEDIUM_MAX = 40,
    ICON_SIZE_LARGE_MIN = 28,
    ICON_SIZE_LARGE_MAX = 44
};

/* Forward declarations for tab callbacks */
static void history_open_latest(InbeApp *app);
static void reset_settings_preview(InbeApp *app);

/* ================================================================
 * TAB BAR DEFINITIONS
 * ================================================================ */

static void on_history_tab_click(void *user_data) {
    InbeApp *app = user_data;
    history_open_latest(app);
    app->inbe.screen = InbeScreenHistory;
}

static void on_manual_tab_click(void *user_data) {
    InbeApp *app = user_data;
    app->tutorial_step = 0;
    app->inbe.screen = InbeScreenManual;
}

static void on_settings_tab_click(void *user_data) {
    InbeApp *app = user_data;
    reset_settings_preview(app);
    app->inbe.screen = InbeScreenSettings;
}

static UITab g_tabs[] = {
    {"History", {0}, on_history_tab_click, NULL},
    {"Manual", {0}, on_manual_tab_click, NULL},
    {"Settings", {0}, on_settings_tab_click, NULL}
};

static UITabBar g_tab_bar = {g_tabs, 3};

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
refresh_theme_colors(int theme_id, int dark_mode)
{
    if (theme_id < 0 || theme_id >= THEME_COUNT)
        theme_id = ThemeSky;

    const ThemeMeta *theme = &g_themes[theme_id];
    const char *scope = dark_mode ? theme->dark_scope : theme->light_scope;

    Color bg = theme_get(scope, "background");
    Color text = theme_get(scope, "text");
    Color circle = theme_get(scope, "circle");
    Color button = theme_get(scope, "button");
    Color button_hover = theme_get(scope, "button_hover");
    Color icon = theme_get(scope, "icon");

    c_bg = bg;
    c_text = text;
    c_circle = circle;
    c_button = button;
    c_button_hover = button_hover;
    c_icon = icon;

    ui_set_colors(text, bg, circle, button, button_hover, icon);
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
register_all_themes(void)
{
    const char *theme_files[] = {
        "themes/sky.ini",
        "themes/sky_dark.ini",
        "themes/ocean.ini",
        "themes/ocean_dark.ini",
        "themes/forest.ini",
        "themes/forest_dark.ini",
        "themes/sunset.ini",
        "themes/sunset_dark.ini",
        "themes/lavender.ini",
        "themes/lavender_dark.ini",
        "themes/cherry.ini",
        "themes/cherry_dark.ini",
        NULL
    };

    const char *scopes[] = {
        "sky_light", "sky_dark",
        "ocean_light", "ocean_dark",
        "forest_light", "forest_dark",
        "sunset_light", "sunset_dark",
        "lavender_light", "lavender_dark",
        "cherry_light", "cherry_dark"
    };

    for (int i = 0; theme_files[i] != NULL; i++) {
        if (theme_scope(scopes[i]) == NULL)
            theme_register_scope(scopes[i], theme_files[i]);
    }
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

    register_all_themes();
    refresh_theme_colors(ThemeSky, 0);  /* Default: Sky light mode */

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
    ui_centered_column(CONTENT_MAX_W, CONTENT_SIDE_PAD, NULL, &content_w);
    update_preview_bounds(&app->settings_preview, content_w, ui_px(132));
    apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
    reset_round_breathe(&app->settings_preview);
}

static void
save_settings(InbeApp *app)
{
    char text[256];
    const char *settings_path =
#if defined(PLATFORM_WEB)
        "/home/settings.ini";
#else
        "settings.ini";
#endif
    snprintf(text, sizeof(text),
             "speed %d\nmax_rounds %d\nmax_breaths %d\npause_seconds %d\nsound_volume %d\ntutorial_seen %d\ntheme %d\ndark_mode %d\nfullscreen %d\n",
             app->inbe.speed_level,
             app->inbe.max_rounds,
             int_from_count(app->inbe.maxbreaths),
             app->inbe.pause_seconds,
             app->sound_volume,
             app->tutorial_seen ? 1 : 0,
             app->theme_id,
             app->dark_mode,
             app->fullscreen_enabled ? 1 : 0);
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
    app->theme_id = clampi(rini_get_value_fallback(settings, "theme", 0), 0, THEME_COUNT - 1);
    app->dark_mode = rini_get_value_fallback(settings, "dark_mode", 0) != 0;
    app->fullscreen_enabled = rini_get_value_fallback(settings, "fullscreen", 0) != 0;
    app->sound_volume = clampi(sound_volume, SETTINGS_VOLUME_MIN, SETTINGS_VOLUME_MAX);

    refresh_theme_colors(app->theme_id, app->dark_mode);
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

    snprintf(path, sizeof(path), "icons/%s", name);
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
    if (app == NULL) return;

    if (app->inbe.screen != InbeScreenSession || app->session_paused) {
        remember_sound_state(app);
        return;
    }

    bool screen_changed = (app->sound_last_screen != InbeScreenSession);
    bool phase_changed  = (app->sound_last_phase != app->inbe.phase);
    bool dir_changed    = (app->sound_last_dir != app->inbe.dir);
    bool count_changed  = !(app->sound_last_count[0] == app->inbe.count[0] &&
                             app->sound_last_count[1] == app->inbe.count[1] &&
                             app->sound_last_count[2] == app->inbe.count[2]);

    if (app->inbe.phase == InbePhaseBreathe) {
        if (screen_changed || phase_changed || dir_changed) {
            Sound breath_snd = (app->inbe.dir == 0) ? app->breath_in_sound : app->breath_out_sound;
            play_app_sound(app, breath_snd, 1.0f);
        }
        if (count_changed) {
            int count_value = int_from_count(app->inbe.count);
            int maxbreaths_value = int_from_count(app->inbe.maxbreaths);
            if (count_value == maxbreaths_value - 1) {
                play_app_sound(app, app->bell_sound, 0.8f);
            }
        }
    }
    else if (phase_changed) {
        switch (app->inbe.phase) {
            case InbePhaseHold:
                break;
                
            case InbePhaseRecover:
                play_app_sound(app, app->breath_in_sound, 1.0f);
                break;
                
            case InbePhaseNext:
                if (app->sound_last_phase == InbePhaseRecover) {
                    play_app_sound(app, app->breath_out_sound, 1.0f);
                }
                break;
                
            default:
                break;
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
    update_circle_bounds_for_view(&app->inbe, 0, ui_clamp_px(TAB_BAR_H, 54, 66) + 80);
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

static int
draw_history_session_row(InbeApp *app, int x, int y, int w, int h, const char *text, int selected)
{
    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
    int mx = (int)mouse_world.x;
    int my = (int)mouse_world.y;
    int hover = 0;
    int icon_size = ui_clamp_px(ICON_SIZE_SMALL, ICON_SIZE_SMALL_MIN, ICON_SIZE_SMALL_MAX);
    int font = ui_clamp_px(14, 12, 16);
    (void)font; /* Currently unused but may be needed for future */

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

    /* Draw text */
    DrawText(text, x + ui_px(46), y + ui_px(6), font, c_text);

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
            DrawTexturePro(app->trash_icon, src, dst, (Vector2){0}, 0, ui_darken(c_icon, 30));
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
        ui_draw_bevel(x, y, w, h, ui_darken(c_button_hover, 40), ui_lighten(c_button_hover, 40));
        *hover = 1;
        app->cursor_clickable = 1;
        if(mb) {
            ui_draw_bevel(x, y, w, h, ui_lighten(c_button_hover, 40), ui_darken(c_button_hover, 40));
        }
        if(released) {
            pressed = 1;
        }
    } else {
        DrawRectangle(x, y, w, h, c_button);
        ui_draw_bevel(x, y, w, h, ui_lighten(c_button, 40), ui_darken(c_button, 40));
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
    int padding = ui_px(10);
    int w = size + padding * 2;
    int h = size + padding * 2;

    int pressed = 0;

    if(mx > x && mx < x + w && my > y && my < y + h) {
        DrawRectangle(x, y, w, h, c_button_hover);
        ui_draw_bevel(x, y, w, h, ui_darken(c_button_hover, 40), ui_lighten(c_button_hover, 40));
        *hover = 1;
        app->cursor_clickable = 1;
        if(mb) {
            ui_draw_bevel(x, y, w, h, ui_lighten(c_button_hover, 40), ui_darken(c_button_hover, 40));
        }
        if(released) {
            pressed = 1;
        }
    } else {
        DrawRectangle(x, y, w, h, c_button);
        ui_draw_bevel(x, y, w, h, ui_lighten(c_button, 40), ui_darken(c_button, 40));
        *hover = 0;
    }

    if(icon.id != 0) {
        Rectangle src = {0, 0, icon.width, icon.height};
        Rectangle dst = {x + padding, y + padding, (float)size, (float)size};
        DrawTexturePro(icon, src, dst, (Vector2){0}, 0, c_icon);
    }

    return pressed;
}


static int
nav_button_width(const char *label, int icon_size, int show_label, int font)
{
    int padding = ui_px(6);
    int width = icon_size + padding * 2;

    if(show_label && label != NULL && label[0] != '\0')
        width += ui_px(10) + MeasureText(label, font);
    return width;
}

static void
draw_icon_link(InbeApp *app, int x, int y, int icon_size, Texture2D icon, const char *url)
{
    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
    int mx = (int)mouse_world.x;
    int my = (int)mouse_world.y;
    int hover = 0;
    int padding = ui_px(4);
    int btn_w = icon_size + padding * 2;
    int btn_h = icon_size + padding * 2;
    int btn_x = x - padding;
    int btn_y = y - padding;

    if(mx > btn_x && mx < btn_x + btn_w && my > btn_y && my < btn_y + btn_h) {
        hover = 1;
        app->cursor_clickable = 1;
    }

    /* Draw button background with bevel */
    if(hover) {
        DrawRectangle(btn_x, btn_y, btn_w, btn_h, c_button_hover);
        ui_draw_bevel(btn_x, btn_y, btn_w, btn_h, ui_darken(c_button_hover, 40), ui_lighten(c_button_hover, 40));
    } else {
        DrawRectangle(btn_x, btn_y, btn_w, btn_h, c_button);
        ui_draw_bevel(btn_x, btn_y, btn_w, btn_h, ui_lighten(c_button, 40), ui_darken(c_button, 40));
    }

    if(icon.id != 0) {
        Rectangle src = {0, 0, icon.width, icon.height};
        Rectangle dst = {x, y, (float)icon_size, (float)icon_size};
        DrawTexturePro(icon, src, dst, (Vector2){0}, 0, c_icon);
    }

    if(hover && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        OpenURL(url);
    }
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
    int padding = ui_px(6);
    int w = nav_button_width(label, icon_size, show_label, font);
    int h = icon_size + padding * 2;
    int pressed = 0;

    if(mx > x && mx < x + w && my > y && my < y + h) {
        DrawRectangle(x, y, w, h, c_button_hover);
        ui_draw_bevel(x, y, w, h, ui_darken(c_button_hover, 40), ui_lighten(c_button_hover, 40));
        *hover = 1;
        app->cursor_clickable = 1;
        if(mb)
            ui_draw_bevel(x, y, w, h, ui_lighten(c_button_hover, 40), ui_darken(c_button_hover, 40));
        if(released)
            pressed = 1;
    } else {
        DrawRectangle(x, y, w, h, c_button);
        ui_draw_bevel(x, y, w, h, ui_lighten(c_button, 40), ui_darken(c_button, 40));
        *hover = 0;
    }

    if(icon.id != 0) {
        Rectangle src = {0, 0, icon.width, icon.height};
        Rectangle dst = {x + padding, y + padding, (float)icon_size, (float)icon_size};
        DrawTexturePro(icon, src, dst, (Vector2){0}, 0, c_icon);
    }

    if(show_label && label != NULL && label[0] != '\0') {
        int text_x = x + icon_size + padding * 2 + ui_px(10);
        int text_y = y + (h - font) / 2;
        DrawText(label, text_x, text_y, font, c_text);
    }

    return pressed;
}

static void
draw_tab_bar(InbeApp *app)
{
    int bar_h = ui_clamp_px(TAB_BAR_H, 54, 66);
    int bar_y = view_height - bar_h;
    int button_size = ui_clamp_px(ICON_SIZE_LARGE, ICON_SIZE_LARGE_MIN, ICON_SIZE_LARGE_MAX);
    int button_h = button_size + ui_px(12);
    int font = ui_clamp_px(14, 12, 16);
    int side_margin = ui_px(16);
    int group_gap = ui_px(10);
    int available_w = view_width - side_margin * 2;

    /* Calculate widths with labels */
    int stat_w_label = nav_button_width("History", button_size, 1, font);
    int manual_w_label = nav_button_width("Manual", button_size, 1, font);
    int gear_w_label = nav_button_width("Settings", button_size, 1, font);
    int group_w_label = stat_w_label + manual_w_label + gear_w_label + group_gap * 2;

    /* Calculate widths without labels */
    int stat_w_no_label = nav_button_width("History", button_size, 0, font);
    int manual_w_no_label = nav_button_width("Manual", button_size, 0, font);
    int gear_w_no_label = nav_button_width("Settings", button_size, 0, font);
    int group_w_no_label = stat_w_no_label + manual_w_no_label + gear_w_no_label + group_gap * 2;

    /* Only show labels if all buttons with labels fit */
    int show_labels = group_w_label <= available_w;
    int stat_w = show_labels ? stat_w_label : stat_w_no_label;
    int manual_w = show_labels ? manual_w_label : manual_w_no_label;
    int gear_w = show_labels ? gear_w_label : gear_w_no_label;
    int group_w = show_labels ? group_w_label : group_w_no_label;

    int group_x, button_y;

    /* Center the button group, but don't exceed margins */
    group_x = side_margin + (available_w - group_w) / 2;
    button_y = bar_y + (bar_h - button_h) / 2;
    int tab_hover = 0;

    DrawRectangle(0, bar_y, view_width, bar_h, ui_darken(c_bg, 10));
    DrawLine(0, bar_y, view_width, bar_y, ui_darken(c_bg, 42));

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

static void
draw_theme_selector(InbeApp *app, int x, int y, int w)
{
    int font = ui_clamp_px(14, 12, 16);
    int small_font = ui_clamp_px(12, 10, 14);
    const char *label = "Theme";

    /* Draw label */
    DrawText(label, x, y, font, c_text);

    /* Light/Dark toggle */
    int toggle_w = ui_px(100);
    int toggle_h = ui_px(28);
    int toggle_x = x + w - toggle_w;
    int toggle_y = y - 2;

    if(ui_draw_toggle_switch(app, toggle_x, toggle_y, toggle_w, toggle_h, &app->dark_mode)) {
        refresh_theme_colors(app->theme_id, app->dark_mode);
        app->settings_dirty = 1;
    }

    /* Theme circles in 2 rows, 3 per row */
    int circle_size = ui_px(36);
    int circle_spacing = ui_px(24);
    int row_spacing = ui_px(36);
    int per_row = 3;
    int row_width = per_row * circle_size + (per_row - 1) * circle_spacing;
    int start_x = x + (w - row_width) / 2;
    int circle_y = y + ui_px(48);
    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);

    for(int i = 0; i < THEME_COUNT; i++) {
        int row = i / per_row;
        int col = i % per_row;
        int cx = start_x + col * (circle_size + circle_spacing) + circle_size / 2;
        int cy = circle_y + row * (circle_size + row_spacing);

        /* Draw circle - get color from Lotus */
        const char *scope = app->dark_mode ? g_themes[i].dark_scope : g_themes[i].light_scope;
        Color theme_color = theme_get(scope, "circle");
        DrawCircle(cx, cy, circle_size / 2, theme_color);

        /* Draw selection ring */
        if(app->theme_id == i) {
            DrawCircleLines(cx, cy, circle_size / 2 + 2, c_text);
        } else {
            DrawCircleLines(cx, cy, circle_size / 2 + 1, ui_darken(c_bg, 30));
        }

        /* Check for click */
        Rectangle bounds = {cx - circle_size / 2 - 4, cy - circle_size / 2 - 4, circle_size + 8, circle_size + 8};
        if(CheckCollisionPointRec(mouse_world, bounds) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            app->theme_id = i;
            refresh_theme_colors(app->theme_id, app->dark_mode);
            app->settings_dirty = 1;
        }

        if(CheckCollisionPointRec(mouse_world, bounds))
            app->cursor_clickable = 1;

        /* Draw theme name below */
        const char *name = g_themes[i].name;
        int name_w = MeasureText(name, small_font);
        DrawText(name, cx - name_w / 2, cy + circle_size / 2 + ui_px(6), small_font, c_text);
    }
}

static void
draw_settings(InbeApp *app)
{
    int title_h = ui_clamp_px(SETTINGS_TITLE_H, 48, 60);
    int viewport_h = view_height - title_h;
    int scaled_content_h = ui_px(SETTINGS_CONTENT_H);
    int max_scroll = scaled_content_h - viewport_h;
    int gear_hover = 0;
    int content_x;
    int content_w;

    if(max_scroll < 0)
        max_scroll = 0;

    ui_centered_column(CONTENT_MAX_W, CONTENT_SIDE_PAD, &content_x, &content_w);

    app->settings_scroll -= (int)(GetMouseWheelMove() * 24.0f);
    app->settings_scroll = clampi(app->settings_scroll, 0, max_scroll);

    DrawRectangle(0, 0, view_width, title_h, ui_darken(c_bg, 14));
    DrawLine(0, title_h - 1, view_width, title_h - 1, ui_darken(c_bg, 42));

    /* Center the "Settings" text vertically */
    int title_font = ui_clamp_px(16, 14, 18);
    int title_text_w = MeasureText("Settings", title_font);
    int title_y = (title_h - title_font) / 2;
    DrawText("Settings", (view_width - title_text_w) / 2, title_y, title_font, c_text);

    if(drawiconbtn(app, view_width - ui_px(40), ui_px(8), ui_clamp_px(18, 16, 40), app->x_icon, &gear_hover)) {
        if(app->settings_dirty)
            save_settings(app);
        app->inbe.screen = InbeScreenStart;
        app->settings_scroll = 0;
    }

    /* Dropdown for tab selection */
    int dropdown_h = ui_px(36);
    int dropdown_y = title_h + ui_px(8);

    BeginScissorMode((int)app->camera.offset.x,
                     (int)(app->camera.offset.y + title_h * app->camera.zoom),
                     (int)(view_width * app->camera.zoom),
                     (int)(viewport_h * app->camera.zoom));
        /* Draw dropdown button (scrolls with content) */
        if(ui_draw_dropdown_button(app, 100, content_x, dropdown_y, content_w, dropdown_h,
                                   settings_tab_names, SETTINGS_TAB_COUNT, &app->settings_tab)) {
            reset_settings_preview(app);
        }

        int yoff = title_h - app->settings_scroll;
        int speed = app->inbe.speed_level;
        int max_rounds = app->inbe.max_rounds;
        int max_breaths = int_from_count(app->inbe.maxbreaths);
        int pause_seconds = app->inbe.pause_seconds;
        int sound_volume = app->sound_volume;

        /* Update preview for breathing tab */
        update_preview_bounds(&app->settings_preview, content_w, ui_px(240));
        apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
        inbestep(&app->settings_preview);
        if(app->settings_preview.phase != InbePhaseBreathe) {
            reset_settings_preview(app);
            update_preview_bounds(&app->settings_preview, content_w, ui_px(240));
            apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
        }

        /* Content starts after dropdown */
        int content_start_y = dropdown_y + dropdown_h + ui_px(8);

        switch(app->settings_tab) {
            case SETTINGS_TAB_BREATHING: {
                /* Circle preview */
                draw_preview_inbe(&app->settings_preview, content_x + content_w / 2, yoff + content_start_y + ui_px(100));

                /* Speed slider */
                if(ui_draw_slider(app, 1, content_x, yoff + content_start_y + ui_px(200), content_w, "Speed", SETTINGS_SPEED_MIN,
                               SETTINGS_SPEED_MAX, &speed, "")) {
                    apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
                    apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
                    app->settings_dirty = 1;
                }
                break;
            }
            case SETTINGS_TAB_SESSION: {
                int slider_y = yoff + content_start_y + ui_px(20);

                /* Max rounds */
                if(ui_draw_slider(app, 2, content_x, slider_y, content_w, "Max rounds", 1,
                               MaxRounds, &max_rounds, "")) {
                    apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
                    apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
                    app->settings_dirty = 1;
                }

                /* Max breaths */
                if(ui_draw_slider(app, 3, content_x, slider_y + ui_px(66), content_w, "Max breaths", SETTINGS_BREATHS_MIN,
                               SETTINGS_BREATHS_MAX, &max_breaths, "")) {
                    apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
                    apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
                    app->settings_dirty = 1;
                }

                /* Pause */
                if(ui_draw_slider(app, 4, content_x, slider_y + ui_px(132), content_w, "Pause after round", SETTINGS_PAUSE_MIN,
                               SETTINGS_PAUSE_MAX, &pause_seconds, "s")) {
                    apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
                    apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
                    app->settings_dirty = 1;
                }

                /* Volume */
                int sound_volume = app->sound_volume;
                if(ui_draw_slider(app, 6, content_x, slider_y + ui_px(198), content_w, "Volume", SETTINGS_VOLUME_MIN,
                               SETTINGS_VOLUME_MAX, &sound_volume, "")) {
                    app->sound_volume = sound_volume;
                    app->settings_dirty = 1;
                }

                /* Reset to defaults button */
                int reset_y = slider_y + ui_px(265);
                int reset_w = MeasureText("Reset to defaults", ui_clamp_px(14, 12, 16)) + ui_px(24);
                int reset_h = ui_px(36);
                int reset_x = content_x + content_w - reset_w;
                int reset_hover = 0;
                Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);

                Rectangle reset_bounds = {reset_x, reset_y, reset_w, reset_h};
                if(CheckCollisionPointRec(mouse_world, reset_bounds)) {
                    DrawRectangle(reset_x, reset_y, reset_w, reset_h, c_button_hover);
                    ui_draw_bevel(reset_x, reset_y, reset_w, reset_h, ui_darken(c_button_hover, 40), ui_lighten(c_button_hover, 40));
                    reset_hover = 1;
                    app->cursor_clickable = 1;
                } else {
                    DrawRectangle(reset_x, reset_y, reset_w, reset_h, c_button);
                    ui_draw_bevel(reset_x, reset_y, reset_w, reset_h, ui_lighten(c_button, 40), ui_darken(c_button, 40));
                }

                int reset_font = ui_clamp_px(14, 12, 16);
                DrawText("Reset to defaults", reset_x + ui_px(12), reset_y + reset_h / 2 - reset_font / 2 - 1, reset_font, c_text);

                if(reset_hover && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                    /* Reset to default values */
                    speed = 6;
                    max_rounds = DefaultMaxRounds;
                    max_breaths = DefaultMaxBreaths;
                    pause_seconds = DefaultPauseSeconds;
                    apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
                    apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
                    app->settings_dirty = 1;
                }
                break;
            }
            case SETTINGS_TAB_APPEARANCE: {
#if !defined(PLATFORM_ANDROID) && !defined(__ANDROID__) && !defined(ANDROID) && !defined(PLATFORM_WEB)
                /* Fullscreen toggle - shown first (desktop only) */
                int checkbox_y = yoff + content_start_y;
                if(ui_draw_checkbox_toggle(app, content_x, checkbox_y, "Fullscreen", &app->fullscreen_enabled)) {
                    if(app->fullscreen_enabled && !IsWindowFullscreen())
                        ToggleFullscreen();
                    else if(!app->fullscreen_enabled && IsWindowFullscreen())
                        ToggleFullscreen();
                    app->settings_dirty = 1;
                }

                /* Theme selector - below fullscreen */
                int theme_y = yoff + content_start_y + ui_px(50);
#else
                /* No fullscreen on Android/Web - theme selector at top */
                int theme_y = yoff + content_start_y;
#endif
                draw_theme_selector(app, content_x, theme_y, content_w);

                /* Language placeholder */
                int font = ui_clamp_px(14, 12, 16);
                int label_y = theme_y + ui_px(220);
                DrawText("Language", content_x, label_y, font, c_text);
                DrawText("Coming soon...", content_x, label_y + ui_px(30), font, ui_darken(c_text, 40));
                break;
            }
            case SETTINGS_TAB_ABOUT: {
                int font = ui_clamp_px(14, 12, 16);
                int small_font = ui_clamp_px(12, 10, 14);
                int text_y = yoff + content_start_y;

                /* App description */
                const char *desc_lines[] = {
                    "Inner Breeze is a simple breathing",
                    "meditation app to help you relax",
                    "and find your calm."
                };
                for(int i = 0; i < 3; i++) {
                    DrawText(desc_lines[i], content_x, text_y, font, c_text);
                    text_y += ui_px(22);
                }

                /* Version info */
                text_y += ui_px(20);
                char version_text[32];
                snprintf(version_text, sizeof(version_text), "Version %s", INBE_VERSION_STRING);
                DrawText(version_text, content_x, text_y, small_font, ui_darken(c_text, 40));

                /* Icon links */
                int links_y = text_y + ui_px(40);
                int icon_size = ui_clamp_px(ICON_SIZE_LARGE, ICON_SIZE_LARGE_MIN, ICON_SIZE_LARGE_MAX);
                int icon_padding = ui_px(4);
                int icon_spacing = ui_px(20);
                int icon_btn_w = icon_size + icon_padding * 2;
                int total_w = icon_btn_w * 4 + icon_spacing * 3;
                int links_start_x = content_x + (content_w - total_w) / 2;
                draw_icon_link(app, links_start_x + icon_padding, links_y, icon_size, app->telegram_icon, "https://t.me/lotusinbe");
                draw_icon_link(app, links_start_x + icon_btn_w + icon_spacing + icon_padding, links_y, icon_size, app->globe_icon, "https://inbe.waozi.xyz/");
                draw_icon_link(app, links_start_x + (icon_btn_w + icon_spacing) * 2 + icon_padding, links_y, icon_size, app->monero_icon, "https://trocador.app/en/anonpay/?ticker_to=xmr&network_to=Mainnet&address=86CbC3d4a2GhT9auh6X99JhmhTMFKVVk8Q9cLrKTHkBu8LLkoNWgkBeAT3YZrvDM6NczYe8brUJNsTiFmwpWDZYnFG5kzSH&donation=True&simple_mode=True&amount=0.1&name=Inner+Breeze&email=waotzi@proton.me&ticker_from=xmr&network_from=Mainnet&buttonbgcolor=445588&textcolor=ffffff&bgcolor=eaeaffff");
                draw_icon_link(app, links_start_x + (icon_btn_w + icon_spacing) * 3 + icon_padding, links_y, icon_size, app->stripe_icon, "https://donate.stripe.com/4gM3cv5boaR98HH9VvfAc04");
                break;
            }
        }
    EndScissorMode();

    /* Draw dropdown menu (floats above content) */
    ui_draw_dropdown_menu(app, 100);

    ui_draw_scrollbar(app, &app->settings_scroll, scaled_content_h, viewport_h,
                   &app->settings_drag_scrollbar, &app->settings_drag_content, &app->settings_drag_content_y);

    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        app->settings_drag_slider = 0;
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
draw_tutorial_image_placeholder(const char *label, int x, int y, int w, int h)
{
    DrawRectangle(x, y, w, h, ui_darken(c_bg, 12));
    ui_draw_bevel(x, y, w, h, ui_darken(c_bg, 45), ui_lighten(c_bg, 35));
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

    DrawRectangle(x, y, w, h, ui_darken(c_bg, 12));
    ui_draw_bevel(x, y, w, h, ui_darken(c_bg, 45), ui_lighten(c_bg, 35));
    DrawTexturePro(texture, src, dst, (Vector2){0}, 0, WHITE);
}

static void
draw_manual(InbeApp *app)
{
    int title_h = ui_clamp_px(SETTINGS_TITLE_H, 48, 60);
    int tab_h = ui_clamp_px(TAB_BAR_H, 54, 66);
    int viewport_h = view_height - title_h - tab_h;
    int content_h = 430;
    int title_font = ui_clamp_px(16, 14, 18);
    int body_font = ui_clamp_px(13, 11, 14);
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

    max_scroll = ui_px(content_h) - viewport_h;
    if(max_scroll < 0)
        max_scroll = 0;
    app->manual_scroll -= (int)(GetMouseWheelMove() * 24.0f);
    app->manual_scroll = clampi(app->manual_scroll, 0, max_scroll);

    ui_centered_column(CONTENT_MAX_W, CONTENT_SIDE_PAD, &content_x, &content_w);

    DrawRectangle(0, 0, view_width, title_h, ui_darken(c_bg, 14));
    DrawLine(0, title_h - 1, view_width, title_h - 1, ui_darken(c_bg, 42));
    title_w = MeasureText(title, title_font);
    int title_y = (title_h - title_font) / 2;
    DrawText(title, view_width / 2 - title_w / 2, title_y, title_font, c_text);

    int x_hover = 0;
    if(drawiconbtn(app, view_width - ui_px(40), ui_px(8), ui_clamp_px(18, 16, 40), app->x_icon, &x_hover))
        tutorial_close(app, 1);

    BeginScissorMode((int)app->camera.offset.x,
                     (int)(app->camera.offset.y + title_h * app->camera.zoom),
                     (int)(view_width * app->camera.zoom),
                     (int)(viewport_h * app->camera.zoom));
        int y = title_h + ui_px(16) - app->manual_scroll;
        if(app->tutorial_step == 0) {
            const char *lines[] = {
                "This breathing practice is based on",
                "the Wim Hof Method.",
                "",
                "It can be powerful. Use it with care.",
                "",
                "Practice sitting or lying down.",
                "Never use it while driving,",
                "standing, or in water."
            };
            int img_h = ui_px(240);
            draw_tutorial_image(app->angel_image, "angel.jpg", content_x, y, content_w, img_h);
            y += img_h + ui_px(22);
            ui_draw_text_lines(lines, 8, content_x, &y, body_font, ui_px(20));
        } else if(app->tutorial_step == 1) {
            const char *lines[] = {
                "Simply follow 4 steps:",
                "",
                "1. Breathe rhythmically.",
                "2. Exhale and hold.",
                "3. Inhale deeply and hold.",
                "4. Exhale and repeat."
            };
            ui_draw_text_lines(lines, 6, content_x, &y, body_font, ui_px(19));
            y += ui_px(12);

            const char *before_gear = "Use the gear icon";
            DrawText(before_gear, content_x, y, body_font, c_text);

            if(app->gear_icon.id != 0) {
                int icon_size = ui_px(14);
                int gear_y = y - icon_size / 2 + ui_px(5);
                int gear_x = content_x + MeasureText(before_gear, body_font) + ui_px(4);
                Rectangle src = {0, 0, app->gear_icon.width, app->gear_icon.height};
                Rectangle dst = {gear_x, gear_y, icon_size, icon_size};
                DrawTexturePro(app->gear_icon, src, dst, (Vector2){0}, 0, c_icon);
            }

            const char *after_gear = " to adjust rounds,";
            DrawText(after_gear, content_x + MeasureText(before_gear, body_font) + ui_px(4) + ui_px(14) + ui_px(4), y, body_font, c_text);
            y += ui_px(19);

            const char *settings_lines2[] = {
                "breaths, speed, and pauses."
            };
            ui_draw_text_lines(settings_lines2, 1, content_x, &y, body_font, ui_px(19));
        } else if(app->tutorial_step == 2) {
            int speed = app->inbe.speed_level;
            const char *lines[] = {
                "Fill your lungs fully, then",
                "let the breath flow out.",
                "",
                "Use this slider to set the",
                "pace of the breathing circle."
            };
            ui_draw_text_lines(lines, 5, content_x, &y, body_font, ui_px(19));
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

            if(ui_draw_slider(app, 10, content_x, y, content_w, "Speed", SETTINGS_SPEED_MIN,
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
            ui_draw_text_lines(lines, 5, content_x, &y, body_font, ui_px(20));
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
            ui_draw_text_lines(lines, 6, content_x, &y, body_font, ui_px(20));
        }
    EndScissorMode();

    ui_draw_scrollbar(app, &app->manual_scroll, ui_px(content_h), viewport_h,
                   &app->manual_drag_scrollbar, &app->manual_drag_content, &app->manual_drag_content_y);
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
        if(app->settings_dirty)
            save_settings(app);
    }
}

static void
draw_history(InbeApp *app)
{
    HistoryEntry entries[HISTORY_MAX_SESSIONS];
    int count = 0;
    int title_h = ui_clamp_px(SETTINGS_TITLE_H, 48, 60);
    int tab_h = ui_clamp_px(TAB_BAR_H, 54, 66);
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

    ui_centered_column(CONTENT_MAX_W, CONTENT_SIDE_PAD, &content_x, &content_w);

    DrawRectangle(0, 0, view_width, title_h, ui_darken(c_bg, 14));
    DrawLine(0, title_h - 1, view_width, title_h - 1, ui_darken(c_bg, 42));

    /* Center the "History" text */
    int title_font = ui_clamp_px(16, 14, 18);
    int title_text_w = MeasureText("History", title_font);
    DrawText("History", (view_width - title_text_w) / 2, ui_px(11), title_font, c_text);

    if(drawiconbtn(app, view_width - ui_px(40), ui_px(8), ui_clamp_px(18, 16, 40), app->x_icon, &close_hover)) {
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
                            DrawRectangle(content_x, y, content_w, row_h, ui_darken(c_bg, 4));
                            ui_draw_bevel(content_x, y, content_w, row_h, ui_darken(c_bg, 24), ui_lighten(c_bg, 14));
                            DrawText(round_label, content_x + ui_px(46), y + ui_px(6), ui_clamp_px(14, 12, 16), c_text);
                            y += row_h;
                        }
                    }
                }
            }
        }
    EndScissorMode();

    ui_draw_scrollbar(app, &app->history_scroll, content_h, viewport_h,
                   &app->history_drag_scrollbar, &app->history_drag_content, &app->history_drag_content_y);
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
    update_circle_bounds_for_view(&app->inbe, ui_clamp_px(SETTINGS_TITLE_H, 48, 60),
                                  ui_clamp_px(TAB_BAR_H, 54, 66) + ui_px(80));
    load_settings(app);
    update_circle_bounds_for_view(&app->inbe, ui_clamp_px(SETTINGS_TITLE_H, 48, 60),
                                  ui_clamp_px(TAB_BAR_H, 54, 66) + 80);
    prepare_history_storage();
    init_audio(app);
    app->camera = (Camera2D){0};
    app->cursor_clickable = 0;
    app->settings_scroll = 0;
    app->settings_drag_slider = 0;
    app->settings_drag_scrollbar = 0;
    app->settings_drag_content = 0;
    app->settings_drag_content_y = 0;
    app->settings_dirty = 0;
    app->settings_tab = SETTINGS_TAB_BREATHING;
    app->manual_scroll = 0;
    app->manual_drag_scrollbar = 0;
    app->manual_drag_content = 0;
    app->manual_drag_content_y = 0;
    app->tutorial_step = 0;
    app->history_scroll = 0;
    app->history_drag_scrollbar = 0;
    app->history_drag_content = 0;
    app->history_drag_content_y = 0;
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
    /* Update tab bar icons */
    g_tabs[0].icon = app->stat_icon;
    g_tabs[0].user_data = app;
    g_tabs[1].icon = app->manual_icon;
    g_tabs[1].user_data = app;
    g_tabs[2].icon = app->gear_icon;
    g_tabs[2].user_data = app;
    if(app->home_icon.id == 0) {
        app->home_icon = load_icon_texture("home.png");
    }
    if(app->trash_icon.id == 0) {
        app->trash_icon = load_icon_texture("trash.png");
    }
    if(app->telegram_icon.id == 0) {
        app->telegram_icon = load_icon_texture("telegram.png");
    }
    if(app->globe_icon.id == 0) {
        app->globe_icon = load_icon_texture("globe.png");
    }
    if(app->stripe_icon.id == 0) {
        app->stripe_icon = load_icon_texture("stripe.png");
    }


    if(app->monero_icon.id == 0) {
        app->monero_icon = load_icon_texture("monero.png");
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
        update_circle_bounds_for_view(&app->inbe, ui_clamp_px(SETTINGS_TITLE_H, 48, 60),
                                      ui_clamp_px(TAB_BAR_H, 54, 66) + 96);
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
            int play_limit = view_height - ui_clamp_px(TAB_BAR_H, 54, 66) - ui_px(48);
            if(play_y > play_limit)
                play_y = play_limit;
            if (drawbtn(app, center_x, play_y, "PLAY", &hover)) {
            start_session(app);
            }
        }
        ui_draw_tab_bar(g_tab_bar.tabs, g_tab_bar.count, app);
        break;

    case InbeScreenSession:
        if(IsKeyPressed(KEY_BACKSPACE)) {
            inbe_app_init(app);
            break;
        }

        int return_hover = 0;
        if(app->return_icon.id != 0 && drawiconbtn(app, ui_px(12), ui_px(12), ui_clamp_px(ICON_SIZE_SMALL, ICON_SIZE_SMALL_MIN, ICON_SIZE_SMALL_MAX), app->return_icon, &return_hover)) {
            inbe_app_init(app);
            break;
        }

        int back_hover = 0;
        int pause_hover = 0;
        int forward_hover = 0;
        int control_y = view_height - ui_px(50);
        int control_size = ui_clamp_px(ICON_SIZE_SMALL, ICON_SIZE_SMALL_MIN, ICON_SIZE_SMALL_MAX);
        int control_padding = ui_px(10);
        int control_btn_w = control_size + control_padding * 2;
        int control_gap = ui_px(12);
        int pause_x = center_x - control_btn_w / 2;
        int back_x = pause_x - control_btn_w - control_gap;
        int forward_x = pause_x + control_btn_w + control_gap;

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

            ui_centered_column(CONTENT_MAX_W, CONTENT_SIDE_PAD, &box_x, &box_w);
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

            DrawRectangle(box_x, box_y, box_w, ui_px(78), ui_darken(c_bg, 6));
            DrawLine(box_x, box_y + ui_px(26), box_x + box_w, box_y + ui_px(26), ui_darken(c_bg, 30));
            DrawLine(box_x, box_y + ui_px(52), box_x + box_w, box_y + ui_px(52), ui_darken(c_bg, 30));
            DrawText(TextFormat("%d rounds", rounds), box_x + ui_px(10), box_y + ui_px(8), ui_clamp_px(ICON_SIZE_SMALL, ICON_SIZE_SMALL_MIN, ICON_SIZE_SMALL_MAX), c_text);
            DrawText(TextFormat("best %ds", best), box_x + ui_px(10), box_y + ui_px(34), ui_clamp_px(ICON_SIZE_SMALL, ICON_SIZE_SMALL_MIN, ICON_SIZE_SMALL_MAX), c_text);
            DrawText(TextFormat("avg %ds", rounds > 0 ? total / rounds : 0), box_x + ui_px(10), box_y + ui_px(60), ui_clamp_px(ICON_SIZE_SMALL, ICON_SIZE_SMALL_MIN, ICON_SIZE_SMALL_MAX), c_text);

            DrawText("Round times", box_x, ui_px(168), ui_clamp_px(14, 12, 16), ui_darken(c_text, 20));
            for(int i = 0; i < rounds; i++) {
                char row[48];
                int seconds = int_from_count(app->inbe.results[i]);
                snprintf(row, sizeof(row), "Round %d  %ds", i + 1, seconds);
                DrawRectangle(box_x, row_y - 1, box_w, row_h, ui_darken(c_bg, 4));
                DrawLine(box_x, row_y + row_h - 2, box_x + box_w, row_y + row_h - 2, ui_darken(c_bg, 26));
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

    view_width = (int)viewport.width;
    view_height = (int)viewport.height;

    /* Calculate DPI scale based on viewport height vs base design (560) */
    dpi_scale = (float)view_height / (float)INBE_DEFAULT_HEIGHT;
    if(dpi_scale < 1.0f)
        dpi_scale = 1.0f;

    ui_init(view_width, view_height, dpi_scale);

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

static void SafeUnloadTexture(Texture2D texture) {
    if (texture.id != 0) {
        UnloadTexture(texture);
    }
}

static void SafeUnloadSound(Sound sound) {
    if (sound.frameCount != 0) {
        UnloadSound(sound);
    }
}

static void
inbe_app_destroy(void *vapp)
{
    InbeApp *app = vapp;
    if (app == NULL) return;

    SafeUnloadTexture(app->gear_icon);
    SafeUnloadTexture(app->x_icon);
    SafeUnloadTexture(app->manual_icon);
    SafeUnloadTexture(app->return_icon);
    SafeUnloadTexture(app->backward_icon);
    SafeUnloadTexture(app->forward_icon);
    SafeUnloadTexture(app->play_icon);
    SafeUnloadTexture(app->pause_icon);
    SafeUnloadTexture(app->stat_icon);
    SafeUnloadTexture(app->home_icon);
    SafeUnloadTexture(app->trash_icon);
    SafeUnloadTexture(app->telegram_icon);
    SafeUnloadTexture(app->globe_icon);
    SafeUnloadTexture(app->monero_icon);
    SafeUnloadTexture(app->stripe_icon);
    SafeUnloadTexture(app->angel_image);
    SafeUnloadTexture(app->begin_image);

    // Unload Sounds
    SafeUnloadSound(app->breath_in_sound);
    SafeUnloadSound(app->breath_out_sound);
    SafeUnloadSound(app->bell_sound);

    if (app->audio_ready) {
        CloseAudioDevice();
        app->audio_ready = 0;
    }

    free(app);
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
