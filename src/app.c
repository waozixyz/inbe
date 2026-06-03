#include "app.h"
#include "data.h"
#include "locale.h"
#include "tabs/language_tab.h"
#include "tabs/history_tab.h"
#include "tabs/manual_tab.h"
#include "tabs/settings_tab.h"
#include "lyra_client.h"
#include "theme.h"
#include "theme_meta.h"
#if defined(LOTUS_BUILD)
#include "lotus_settings.h"
#endif
#include "version.h"
#include "ui/ui.h"
#include "ui/dpi.h"
#if !defined(LOTUS_BUILD)
#define RINI_IMPLEMENTATION
#endif
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

#ifdef __ANDROID__
#include "android_wakelock.h"
#include "android_timer.h"
void set_global_inbe_app(InbeApp *app);
#endif

#define INBE_DEFAULT_WIDTH 320
#define INBE_DEFAULT_HEIGHT 560

InbeConfig config = {
    .title = "",
    .width = INBE_DEFAULT_WIDTH,
    .height = INBE_DEFAULT_HEIGHT,
    .loaded = 0,
    .title_custom = 0
};

int view_width = INBE_DEFAULT_WIDTH;
int view_height = INBE_DEFAULT_HEIGHT;
Color c_text, c_bg, c_circle, c_button, c_button_hover, c_icon;

/* Forward declarations for tab callbacks */
void reset_settings_preview(InbeApp *app);

#if defined(LOTUS_BUILD)
static void
sync_lotus_settings(InbeApp *app)
{
    const LotusSettings *lotus;
    unsigned int version;

    if(app == NULL)
        return;

    version = lotus_settings_version();
    if(app->lotus_settings_version == version)
        return;

    lotus = lotus_settings_get();
    app->theme_id = lotus->theme;
    app->dark_mode = lotus->dark_mode ? 1 : 0;
    snprintf(app->language, sizeof(app->language), "%.*s",
             (int)sizeof(app->language) - 1, lotus->language);
    app->language_selected = 1;
    if(!locale_set(app->language)) {
        snprintf(app->language, sizeof(app->language), "%s", "en");
        locale_set(app->language);
    }

    refresh_theme_colors(app->theme_id, app->dark_mode);
    refresh_locale_dependent_text(app);
    app->lotus_settings_version = version;
}
#endif

/* ================================================================
 * TAB BAR DEFINITIONS
 * ================================================================ */

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
    {NULL, {0}, UI_ICON_TYPE_STAT, history_tab_on_click, NULL},
    {NULL, {0}, UI_ICON_TYPE_MANUAL, on_manual_tab_click, NULL},
    {NULL, {0}, UI_ICON_TYPE_GEAR, on_settings_tab_click, NULL}
};

static UITabBar g_tab_bar = {g_tabs, 3};

static void
refresh_tab_labels(void)
{
    g_tabs[0].label = locale_get("tab_history");
    g_tabs[1].label = locale_get("tab_manual");
    g_tabs[2].label = locale_get("tab_settings");
}

void
refresh_locale_dependent_text(InbeApp *app)
{
    if(app == NULL)
        return;

    refresh_tab_labels();
    if(!config.title_custom) {
        snprintf(config.title, sizeof(config.title), "%s", locale_get("app_title"));
    }
    manual_tab_reset_layouts(app);
    app->language_index = locale_current_index();
    if(app->language_index < 0)
        app->language_index = 0;
}

void
apply_language_selection(InbeApp *app, int language_index, int save_now)
{
    const char *code;

    if(app == NULL)
        return;

    if(language_index < 0 || language_index >= locale_count())
        language_index = 0;

    code = locale_code_at(language_index);
    if(code == NULL || code[0] == '\0')
        code = "en";

    if(!locale_set(code)) {
        code = "en";
        locale_set(code);
    }

    snprintf(app->language, sizeof(app->language), "%s", code);
    app->language_selected = 1;
    refresh_locale_dependent_text(app);

    if(save_now)
        save_settings(app);
}

#if defined(PLATFORM_WEB)
#include <emscripten.h>

static int web_storage_ready = 0;

static void
init_web_storage(void)
{
    int ok;

    if(web_storage_ready)
        return;

    ok = EM_ASM_INT({
        if(typeof FS === 'undefined' || typeof IDBFS === 'undefined')
            return 0;

        try {
            FS.mkdir('/home');
        } catch(e) {}

        try {
            if(!FS.analyzePath('/home').object.isFolder) return 0;
            FS.mount(IDBFS, {root: '/'}, '/home');
        } catch(e) {
            if(e.errno !== 10 && String(e).indexOf('already mounted') === -1) {
                console.error('IDBFS mount failed:', e);
                return 0;
            }
        }

        return 1;
    });
    web_storage_ready = ok != 0;
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

void
refresh_theme_colors(int theme_id, int dark_mode)
{
#if defined(LOTUS_BUILD)
    (void)theme_id;
    (void)dark_mode;
    c_bg = lotus_alias_color("background");
    c_text = lotus_alias_color("text");
    c_circle = lotus_alias_color("circle");
    c_button = lotus_alias_color("button");
    c_button_hover = lotus_alias_color("button_hover");
    c_icon = lotus_alias_color("icon");
    ui_set_colors(c_text, c_bg, c_circle, c_button, c_button_hover, c_icon);
    return;
#endif
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

void
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
inbe_settings_path(char *out, size_t out_size)
{
    char apps_path[FS_PATH_MAX];
    char inbe_path[FS_PATH_MAX];
    snprintf(apps_path, sizeof(apps_path), "%s/apps", data_root());
    if(!DirectoryExists(apps_path))
        MakeDirectory(apps_path);
    snprintf(inbe_path, sizeof(inbe_path), "%s/apps/inbe", data_root());
    if(!DirectoryExists(inbe_path))
        MakeDirectory(inbe_path);
    snprintf(out, out_size, "%s/apps/inbe/settings.ini", data_root());
}

static void
inbe_lyra_manifest(LyraClientState *state, LyraClientManifest *manifest, void *user_data)
{
    char settings_path[FS_PATH_MAX];
    (void)user_data;

    manifest->files = (uint64_t)data_get_session_count();
    manifest->bytes = (uint64_t)data_get_total_size();

    if(state->mode == LYRA_CLIENT_MODE_DATA_SETTINGS) {
        inbe_settings_path(settings_path, sizeof(settings_path));
        if(FileExists(settings_path)) {
            int size = GetFileLength(settings_path);
            manifest->files++;
            if(size > 0)
                manifest->bytes += (uint64_t)size;
        }
    }
    manifest->has_data = manifest->files > 0;
}

static void
register_all_themes(void)
{
    for(int i = 0; i < THEME_COUNT; i++) {
        if(theme_scope(g_themes[i].light_scope) == NULL)
            theme_register_scope(g_themes[i].light_scope, g_themes[i].light_path);
        if(theme_scope(g_themes[i].dark_scope) == NULL)
            theme_register_scope(g_themes[i].dark_scope, g_themes[i].dark_path);
    }
}

static void
load_config(void)
{
    if(config.loaded)
        return;

    const char *paths[] = {
        "inbe.ini",
        "apps/inbe/inbe.ini",
        "../inbe/inbe.ini",
        0
    };

    for(int i = 0; paths[i] != 0; i++) {
        rini_data ini = rini_load(paths[i]);
        if(ini.count == 0) {
            rini_unload(&ini);
            continue;
        }

        const char *title = rini_get_value_text(ini, "title");
        if(title != NULL && title[0] != '\0') {
            snprintf(config.title, sizeof(config.title), "%s", title);
            config.title_custom = 1;
        } else {
            snprintf(config.title, sizeof(config.title), "%s", locale_get("app_title"));
            config.title_custom = 0;
        }
        config.width = rini_get_value_fallback(ini, "width", INBE_DEFAULT_WIDTH);
        config.height = rini_get_value_fallback(ini, "height", INBE_DEFAULT_HEIGHT);
        rini_unload(&ini);
        break;
    }

    if(config.title[0] == '\0') {
        snprintf(config.title, sizeof(config.title), "%s", locale_get("app_title"));
        config.title_custom = 0;
    }

    register_all_themes();
    refresh_theme_colors(ThemeSky, 0);  /* Default: Sky light mode */

    config.loaded = 1;
}

int
clampi(int value, int min, int max)
{
    if(value < min)
        return min;
    if(value > max)
        return max;
    return value;
}

void
count_from_int(char dst[CountSize], int value)
{
    value = clampi(value, 0, 999);
    dst[0] = (char)('0' + (value / 100) % 10);
    dst[1] = (char)('0' + (value / 10) % 10);
    dst[2] = (char)('0' + value % 10);
    dst[3] = 0;
}

int
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

void
apply_settings(Inbe *inbe, int speed, int max_rounds, int max_breaths, int pause_seconds)
{
    speed = clampi(speed, SETTINGS_SPEED_MIN, SETTINGS_SPEED_MAX);
    inbe->speed_level = speed;
    inbe->breath_half_ticks = inbe_breath_half_ticks_for_speed(speed);
    inbe->max_rounds = clampi(max_rounds, 1, MaxRounds);
    inbe->pause_seconds = clampi(pause_seconds, SETTINGS_PAUSE_MIN, SETTINGS_PAUSE_MAX);
    count_from_int(inbe->maxbreaths, clampi(max_breaths, SETTINGS_BREATHS_MIN, SETTINGS_BREATHS_MAX));
}

void
reset_settings_preview(InbeApp *app)
{
    int speed = app->inbe.speed_level;
    int max_rounds = app->inbe.max_rounds;
    int max_breaths = int_from_count(app->inbe.maxbreaths);
    int pause_seconds = app->inbe.pause_seconds;
    int play_in_background = app->inbe.play_in_background;
    int content_w;

    inbeinit(&app->settings_preview);
    app->settings_preview.progressive_speed = 0;
    app->settings_preview.play_in_background = play_in_background;
    ui_centered_column(CONTENT_MAX_W, CONTENT_SIDE_PAD, NULL, &content_w);
    update_preview_bounds(&app->settings_preview, content_w, ui_px(132));
    apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
    reset_round_breathe(&app->settings_preview);
}

void
save_settings(InbeApp *app)
{
    char text[1024];
    char settings_path[FS_PATH_MAX];
    inbe_settings_path(settings_path, sizeof(settings_path));
#ifdef __ANDROID__
    snprintf(text, sizeof(text),
             "speed %d\nmax_rounds %d\nmax_breaths %d\npause_seconds %d\nsound_volume %d\ntutorial_seen %d\ntheme %d\ndark_mode %d\nfullscreen %d\non_screen_keyboard %d\nprogressive_speed %d\nadvanced_session_controls %d\nplay_in_background %d\nlanguage %s\nlyra_url %s\nlyra_autoconnect %d\nlyra_mode %d\n",
             app->inbe.speed_level,
             app->inbe.max_rounds,
             int_from_count(app->inbe.maxbreaths),
             app->inbe.pause_seconds,
             app->sound_volume,
             app->tutorial_seen ? 1 : 0,
             app->theme_id,
             app->dark_mode,
             app->fullscreen_enabled ? 1 : 0,
             app->on_screen_keyboard_enabled ? 1 : 0,
             app->inbe.progressive_speed,
             app->advanced_session_controls ? 1 : 0,
             app->inbe.play_in_background,
             (app->language_selected && app->language[0] != '\0')
                 ? app->language
                 : "",
             app->lyra.url,
             app->lyra.auto_connect ? 1 : 0,
             app->lyra.mode == LYRA_CLIENT_MODE_DATA_SETTINGS ? 1 : 0);
#else
    snprintf(text, sizeof(text),
             "speed %d\nmax_rounds %d\nmax_breaths %d\npause_seconds %d\nsound_volume %d\ntutorial_seen %d\ntheme %d\ndark_mode %d\nfullscreen %d\non_screen_keyboard %d\nprogressive_speed %d\nadvanced_session_controls %d\nlanguage %s\nlyra_url %s\nlyra_autoconnect %d\nlyra_mode %d\n",
             app->inbe.speed_level,
             app->inbe.max_rounds,
             int_from_count(app->inbe.maxbreaths),
             app->inbe.pause_seconds,
             app->sound_volume,
             app->tutorial_seen ? 1 : 0,
             app->theme_id,
             app->dark_mode,
             app->fullscreen_enabled ? 1 : 0,
             app->on_screen_keyboard_enabled ? 1 : 0,
             app->inbe.progressive_speed,
             app->advanced_session_controls ? 1 : 0,
             (app->language_selected && app->language[0] != '\0')
                 ? app->language
                 : "",
             app->lyra.url,
             app->lyra.auto_connect ? 1 : 0,
             app->lyra.mode == LYRA_CLIENT_MODE_DATA_SETTINGS ? 1 : 0);
#endif
    SaveFileText(settings_path, text);
#if defined(PLATFORM_WEB)
    sync_web_storage();
#endif
    app->settings_dirty = 0;
}

static void
load_settings(InbeApp *app)
{
    char settings_path[FS_PATH_MAX];
    inbe_settings_path(settings_path, sizeof(settings_path));
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
#ifdef __ANDROID__
    app->on_screen_keyboard_enabled = rini_get_value_fallback(settings, "on_screen_keyboard", 1) != 0;
#else
    app->on_screen_keyboard_enabled = rini_get_value_fallback(settings, "on_screen_keyboard", 0) != 0;
#endif
    app->sound_volume = clampi(sound_volume, SETTINGS_VOLUME_MIN, SETTINGS_VOLUME_MAX);
    app->inbe.progressive_speed = rini_get_value_fallback(settings, "progressive_speed", 1) != 0;
    app->advanced_session_controls = rini_get_value_fallback(settings, "advanced_session_controls", 0) != 0;
    {
        const char *language = rini_get_value_text(settings, "language");
        if(language != NULL && language[0] != '\0') {
            snprintf(app->language, sizeof(app->language), "%s", language);
            app->language_selected = 1;
            if(!locale_set(app->language)) {
                snprintf(app->language, sizeof(app->language), "%s", "en");
                locale_set(app->language);
            }
        } else {
            snprintf(app->language, sizeof(app->language), "%s", "en");
            app->language_selected = 0;
            locale_set(app->language);
        }
        app->language_index = locale_current_index();
        if(app->language_index < 0)
            app->language_index = 0;
    }
#if defined(LOTUS_BUILD)
    sync_lotus_settings(app);
#endif

#ifdef __ANDROID__
    app->inbe.play_in_background = rini_get_value_fallback(settings, "play_in_background",
        1  // Default to enabled on Android
    );
    TraceLog(LOG_INFO, "INBE: Loaded play_in_background setting = %d", app->inbe.play_in_background);
#else
    app->inbe.play_in_background = 0;
#endif

    refresh_theme_colors(app->theme_id, app->dark_mode);
    apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
    refresh_locale_dependent_text(app);
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

void
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
                /* Sound already played in finish_hold() */
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
    int progressive_speed = app->inbe.progressive_speed;
    int play_in_background = app->inbe.play_in_background;

    inbeinit(&app->inbe);
    app->inbe.progressive_speed = progressive_speed;
    app->inbe.play_in_background = play_in_background;
    apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
    /* Save user's pause preference and use 3 seconds for first round */
    app->saved_pause_seconds = app->inbe.pause_seconds;
    app->inbe.pause_seconds = 3;
    update_circle_bounds_for_view(&app->inbe, 0, ui_clamp_px(TAB_BAR_H, 54, 66) + 80);
    app->inbe.screen = InbeScreenSession;
    app->session_paused = 0;
    app->results_saved = 0;
    app->results_path[0] = '\0';
    remember_sound_state(app);

#ifdef __ANDROID__
    TraceLog(LOG_INFO, "INBE: Starting session - play_in_background = %d", app->inbe.play_in_background);
    if (app->inbe.play_in_background) {
        TraceLog(LOG_INFO, "INBE: Acquiring wake lock and starting timer");
        android_wakelock_acquire();
        android_timer_set_app(app);
        set_global_inbe_app(app);  // Update global pointer for JNI
        android_timer_start();
    } else {
        TraceLog(LOG_INFO, "INBE: SKIPPING wake lock and timer (play_in_background disabled)");
        set_global_inbe_app(app);  // Still set pointer for JNI access
    }
#endif
}

static int
collect_result_rounds(InbeApp *app, int *round_times, int max_rounds)
{
    int rounds;
    int count = 0;

    if(app == NULL || round_times == NULL || max_rounds <= 0)
        return 0;

    rounds = app->inbe.round + 1;
    if(rounds < 1)
        rounds = 1;
    if(rounds > app->inbe.max_rounds)
        rounds = app->inbe.max_rounds;
    if(rounds > max_rounds)
        rounds = max_rounds;

    for(int i = 0; i < rounds; i++) {
        int seconds = int_from_count(app->inbe.results[i]);
        if(seconds > 0)
            round_times[count++] = seconds;
    }

    return count;
}

static int
ensure_results_saved(InbeApp *app)
{
    int round_times[MaxRounds];
    int rounds;

    if(app == NULL)
        return 0;
    if(app->results_saved)
        return 1;

    rounds = collect_result_rounds(app, round_times, MaxRounds);
    if(rounds <= 0)
        return 0;

    if(data_save_session_path(round_times, rounds, app->results_path, sizeof(app->results_path))) {
        app->results_saved = 1;
        TraceLog(LOG_INFO, "INBE: session saved successfully");
        return 1;
    }

    TraceLog(LOG_WARNING, "INBE: session save failed");
    return 0;
}

static void
discard_saved_results(InbeApp *app)
{
    if(app == NULL)
        return;

    if(app->results_saved && app->results_path[0] != '\0') {
        if(data_delete_session(app->results_path)) {
            app->results_saved = 0;
            app->results_path[0] = '\0';
        }
    }
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
    /* Play breath-in sound when user finishes hold and starts recovery */
    play_app_sound(app, app->breath_in_sound, 1.0f);
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
        if(ensure_results_saved(app))
            app->inbe.screen = InbeScreenResults;
        else
            inbe_app_init(app);
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
    locale_format(max_text, sizeof(max_text), "starting_in", 30);
    max_text_w = MeasureText(max_text, font);

    locale_format(text, sizeof(text), "starting_in", remaining);
    text_y = center_y - (int)(app->inbe.rmax * 0.72f) - ui_px(40);
    if(text_y < ui_px(20))
        text_y = ui_px(20);
    DrawText(text, center_x - max_text_w / 2, text_y, font, c_text);
}

void
draw_preview_inbe(Inbe *inbe, int center_x, int center_y)
{
    int r = (int)((float)inbe->r * 0.72f);
    DrawCircle(center_x, center_y, r, c_circle);
    DrawCircleLines(center_x, center_y, r, c_text);
}

void
inbe_app_init(void *vapp) {
    InbeApp *app = vapp;
    if(app == 0)
        return;

#ifdef __ANDROID__
    if (app->inbe.play_in_background) {
        android_timer_stop();
        android_wakelock_release();
    }
#endif

#if defined(PLATFORM_WEB)
    init_web_storage();
#endif
    locale_init();
    refresh_tab_labels();
    load_config();

    view_width = config.width > 0 ? config.width : INBE_DEFAULT_WIDTH;
    view_height = config.height > 0 ? config.height : INBE_DEFAULT_HEIGHT;
    dpi_update(view_width, view_height);
    ui_init(view_width, view_height, dpi_ui_scale());

    inbeinit(&app->inbe);
    update_circle_bounds_for_view(&app->inbe, ui_clamp_px(SETTINGS_TITLE_H, 48, 60),
                                  ui_clamp_px(TAB_BAR_H, 54, 66) + ui_px(80));
    load_settings(app);
    {
        char lyra_settings_path[FS_PATH_MAX];
        inbe_settings_path(lyra_settings_path, sizeof(lyra_settings_path));
        lyra_client_set_identity(&app->lyra, "inbe", "Inner Breeze");
        lyra_client_set_settings_path(&app->lyra, lyra_settings_path);
        lyra_client_set_manifest_callback(&app->lyra, inbe_lyra_manifest, NULL);
    }
    lyra_client_load_settings(&app->lyra);
    update_circle_bounds_for_view(&app->inbe, ui_clamp_px(SETTINGS_TITLE_H, 48, 60),
                                  ui_clamp_px(TAB_BAR_H, 54, 66) + 80);
    data_init();
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
    app->tutorial_layouts_initialized = 0;
    history_tab_reset(app);
    app->session_paused = 0;
    app->results_saved = 0;
    app->results_path[0] = '\0';
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
    if(app->trash_icon.id == 0) {
        app->trash_icon = load_icon_texture("trash.png");
    }
    if(app->pencil_icon.id == 0) {
        app->pencil_icon = load_icon_texture("pencil.png");
    }
    if(app->save_icon.id == 0) {
        app->save_icon = load_icon_texture("save.png");
    }
    /* Update tab bar icons */
    g_tabs[0].icon = app->stat_icon;
    g_tabs[0].icon_type = UI_ICON_TYPE_STAT;
    g_tabs[0].user_data = app;
    g_tabs[1].icon = app->manual_icon;
    g_tabs[1].icon_type = UI_ICON_TYPE_MANUAL;
    g_tabs[1].user_data = app;
    g_tabs[2].icon = app->gear_icon;
    g_tabs[2].icon_type = UI_ICON_TYPE_GEAR;
    g_tabs[2].user_data = app;

    if(app->telegram_icon.id == 0) {
        app->telegram_icon = load_icon_texture("telegram.png");
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
#if !defined(LOTUS_BUILD)
    if(!app->language_selected)
        app->inbe.screen = InbeScreenLanguage;
    else if(!app->tutorial_seen)
        app->inbe.screen = InbeScreenManual;
    else
        app->inbe.screen = InbeScreenStart;
#else
    if(!app->tutorial_seen)
        app->inbe.screen = InbeScreenManual;
    else
        app->inbe.screen = InbeScreenStart;
#endif

    /* Reset modal state */
    app->modal.active = 0;
    app->modal.type = UIModalNone;
    app->modal.selected_button = 0;

    if(app->lyra.auto_connect && app->lyra.url[0] != '\0')
        lyra_client_connect(&app->lyra, app->lyra.url);
}

/* Check if session has any completed rounds */
static int
session_has_completed_rounds(InbeApp *app)
{
    int round_times[MaxRounds];
    return collect_result_rounds(app, round_times, MaxRounds) > 0;
}

static void
handle_back_button(InbeApp *app)
{
    /* If modal is active, let modal drawing handle it */
    if(app->modal.active) {
        return;
    }

    switch(app->inbe.screen) {
    case InbeScreenSettings:
        if(app->settings_dirty)
            save_settings(app);
        app->inbe.screen = InbeScreenStart;
        app->settings_scroll = 0;
        break;

    case InbeScreenHistory:
        if(!history_tab_handle_back(app))
            app->inbe.screen = InbeScreenStart;
        break;

    case InbeScreenLanguage:
        break;

    case InbeScreenManual:
        manual_tab_close_tutorial(app, 1);
        break;

    case InbeScreenResults:
        ensure_results_saved(app);
        inbe_app_init(app);
        break;

    case InbeScreenSession:
        /* When paused, exit immediately */
        if(app->session_paused) {
#ifdef __ANDROID__
            if (app->inbe.play_in_background) {
                android_wakelock_release();
                android_timer_stop();
            }
#endif
            inbe_app_init(app);
        } else {
            /* Show confirmation modal */
            app->modal.active = 1;
            app->modal.type = UIModalConfirmExitSession;
            app->modal.selected_button = 0;
        }
        break;

    case InbeScreenStart:
    default:
        break;
    }
}

static void
updateapp(InbeApp *app)
{
    int center_x = view_width / 2;
    int center_y = view_height / 2;
    int hover = 0;
    int modal_result = 0;

    lyra_client_poll(&app->lyra);

    /* Handle Android back button and desktop backspace */
    if(IsKeyPressed(KEY_BACK) ||
       (IsKeyPressed(KEY_BACKSPACE) &&
        !(app->inbe.screen == InbeScreenHistory && history_tab_is_editing(app)))) {
        if(app->modal.active) {
            /* Modal active - close it on cancel/confirm */
            app->modal.active = 0;
            app->modal.type = UIModalNone;
        } else {
            handle_back_button(app);
        }
    }

    if(app->inbe.screen == InbeScreenSettings) {
        settings_tab_draw(app);
        app->inbe.frame++;
        return;
    }

    if(app->inbe.screen == InbeScreenLanguage) {
        language_tab_draw(app);
        app->inbe.frame++;
        return;
    }

    if(app->inbe.screen == InbeScreenManual) {
        manual_tab_draw(app);
        app->inbe.frame++;
        return;
    }

    if(app->inbe.screen == InbeScreenHistory) {
        history_tab_draw(app);
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
            int play_y = center_y + (int)(app->inbe.rmax * dpi_ui_scale() + 0.5f) + ui_px(20);
            int play_limit = view_height - ui_clamp_px(TAB_BAR_H, 54, 66) - ui_px(48);
            if(play_y > play_limit)
                play_y = play_limit;
            if (ui_draw_text_btn(app, center_x, play_y, locale_get("play_button"), &hover)) {
            start_session(app);
            }
        }
        ui_draw_tab_bar(g_tab_bar.tabs, g_tab_bar.count, app);
        break;

    case InbeScreenSession: {
        int return_hover = 0;
        if(ui_draw_icon_btn_padded(app, ui_px(12), ui_px(12), ui_clamp_px(ICON_SIZE_SMALL, ICON_SIZE_SMALL_MIN, ICON_SIZE_SMALL_MAX),
                                   ui_px(10), app->return_icon, UI_ICON_TYPE_RETURN, &return_hover)) {
            handle_back_button(app);
        }

        if(app->modal.active && app->modal.type == UIModalConfirmExitSession) {
            int has_rounds = session_has_completed_rounds(app);

            if(has_rounds) {
                modal_result = ui_draw_modal_3btn(app,
                                                   locale_get("exit_session_title"),
                                                   locale_get("save_completed_rounds_message"),
                                                   locale_get("cancel_button"),
                                                   locale_get("save_button"),
                                                   locale_get("discard_button"));
                if(modal_result == 1) {
                    app->modal.active = 0;
                    app->modal.type = UIModalNone;
                } else if(modal_result == 2) {
                    /* Save - save rounds and exit */
                    ensure_results_saved(app);
#ifdef __ANDROID__
                    if (app->inbe.play_in_background) {
                        android_wakelock_release();
                        android_timer_stop();
                    }
#endif
                    app->modal.active = 0;
                    app->modal.type = UIModalNone;
                    inbe_app_init(app);
                } else if(modal_result == 3) {
                    /* Discard - exit without saving */
#ifdef __ANDROID__
                    if (app->inbe.play_in_background) {
                        android_wakelock_release();
                        android_timer_stop();
                    }
#endif
                    app->modal.active = 0;
                    app->modal.type = UIModalNone;
                    inbe_app_init(app);
                }
            } else {
                /* 2-button modal: Cancel, Exit */
                modal_result = ui_draw_modal(app,
                                             locale_get("exit_session_title"),
                                             locale_get("all_progress_lost_message"),
                                             locale_get("cancel_button"),
                                             locale_get("exit_button"));
                if(modal_result == 1) {
                    /* Cancel - close modal and continue session */
                    app->modal.active = 0;
                    app->modal.type = UIModalNone;
                } else if(modal_result == 2) {
                    /* Exit - no data to save, just exit */
#ifdef __ANDROID__
                    if (app->inbe.play_in_background) {
                        android_wakelock_release();
                        android_timer_stop();
                    }
#endif
                    app->modal.active = 0;
                    app->modal.type = UIModalNone;
                    inbe_app_init(app);
                }
            }
            /* If modal_result == 0, no button clicked, modal stays open */
            break;
        }

        int back_hover = 0;
        int pause_hover = 0;
        int forward_hover = 0;
        int control_y = view_height - ui_px(50);
        int breath_max_y = view_height - ui_px(44);

        if(app->advanced_session_controls) {
        int control_size = ui_clamp_px(ICON_SIZE_SMALL, ICON_SIZE_SMALL_MIN, ICON_SIZE_SMALL_MAX);
        int control_padding = ui_px(10);
        int control_gap = ui_px(12);
        int min_view_dim = view_width < view_height ? view_width : view_height;
        int available_row_w = view_width - ui_px(48);
        int max_btn_w = min_view_dim / 6;
        int max_btn_w_by_row;
        int control_btn_w;
        int pause_x;
        int back_x;
        int forward_x;

        if(available_row_w < ui_px(120))
            available_row_w = ui_px(120);

        max_btn_w_by_row = (available_row_w - control_gap * 2) / 3;
        if(max_btn_w <= 0 || max_btn_w > max_btn_w_by_row)
            max_btn_w = max_btn_w_by_row;

        control_btn_w = control_size + control_padding * 2;
        if(control_btn_w > max_btn_w) {
            control_btn_w = max_btn_w;
            control_padding = control_btn_w / 4;
            control_size = control_btn_w - control_padding * 2;
        }

        if(control_padding < ui_px(6))
            control_padding = ui_px(6);
        if(control_size < ui_px(16))
            control_size = ui_px(16);

        control_btn_w = control_size + control_padding * 2;
        control_gap = control_btn_w / 4;
        if(control_gap < ui_px(8))
            control_gap = ui_px(8);
        control_y = view_height - ui_px(6) - control_btn_w;
        breath_max_y = control_y - ui_px(44);
        pause_x = center_x - control_btn_w / 2;
        back_x = pause_x - control_btn_w - control_gap;
        forward_x = pause_x + control_btn_w + control_gap;

        if(ui_draw_icon_btn_padded(app, back_x, control_y, control_size, control_padding,
                                                     app->backward_icon, UI_ICON_TYPE_BACKWARD, &back_hover)) {
            session_step_back(app);
        }
        if(ui_draw_icon_btn_padded(app, pause_x, control_y, control_size, control_padding,
                       app->session_paused ? app->play_icon : app->pause_icon, app->session_paused ? UI_ICON_TYPE_PLAY : UI_ICON_TYPE_PAUSE, &pause_hover)) {
            app->session_paused = !app->session_paused;
        }
        if(ui_draw_icon_btn_padded(app, forward_x, control_y, control_size, control_padding,
                                                    app->forward_icon, UI_ICON_TYPE_FORWARD, &forward_hover)) {
            session_step_forward(app);
        }
        }

        draw_session_status(app, center_x, center_y);

        if(!app->session_paused) {
#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
            pthread_mutex_t *timer_mutex = android_timer_get_mutex();
            if (timer_mutex) {
                pthread_mutex_lock(timer_mutex);
                inbestep(&app->inbe);
                update_session_sounds(app);
                pthread_mutex_unlock(timer_mutex);
            } else {
                inbestep(&app->inbe);
                update_session_sounds(app);
            }
#else
            inbestep(&app->inbe);
            update_session_sounds(app);
#endif
        }

        if (app->inbe.phase == InbePhaseHold) {
            int breath_y = center_y + (int)(app->inbe.rmax * dpi_ui_scale() + 0.5f) + ui_px(24);
            if(breath_y > breath_max_y)
                breath_y = breath_max_y;
            if (ui_draw_text_btn(app, center_x, breath_y, locale_get("breath_button"), &hover)) {
                finish_hold(app);
            }
        }
        break;
    }

    case InbeScreenResults:
        {
            int box_x;
            int box_y = ui_px(78);
            int box_w;
            int row_y = ui_px(200);
            int row_h = ui_clamp_px(32, 28, 36);
            int total = 0;
            int best = -1;
            int round_times[MaxRounds];
            int rounds = collect_result_rounds(app, round_times, MaxRounds);
            int discard_hover = 0;
            int save_hover = 0;
            int action_y = view_height - ui_px(40);

            if(rounds <= 0) {
                inbe_app_init(app);
                break;
            }

            /* Responsive width like other tabs - not fixed CONTENT_MAX_W */
            int responsive_max_w = (int)(view_width * 0.96f);
            int min_content_w = ui_px(320);
            if(responsive_max_w < min_content_w)
                responsive_max_w = min_content_w;
            int side_padding = ui_page_side_padding();

            ui_centered_column(responsive_max_w, side_padding, &box_x, &box_w);
            title_w = MeasureText(locale_get("results_title"), title_font);
            DrawText(locale_get("results_title"), center_x - title_w / 2, ui_px(34), title_font, c_text);

            for(int i = 0; i < rounds; i++) {
                int seconds = round_times[i];
                total += seconds;
                if(best < 0 || seconds > best)
                    best = seconds;
            }

            if(best < 0)
                best = 0;

            DrawRectangle(box_x, box_y, box_w, ui_px(88), ui_darken(c_bg, 6));
            DrawLine(box_x, box_y + ui_px(29), box_x + box_w, box_y + ui_px(29), ui_darken(c_bg, 30));
            DrawLine(box_x, box_y + ui_px(58), box_x + box_w, box_y + ui_px(58), ui_darken(c_bg, 30));
            {
                char line[64];
                locale_format(line, sizeof(line), "results_rounds", rounds);
                DrawText(line, box_x + ui_px(10), box_y + ui_px(10), ui_clamp_px(ICON_SIZE_SMALL, ICON_SIZE_SMALL_MIN, ICON_SIZE_SMALL_MAX), c_text);
                locale_format(line, sizeof(line), "results_best", best);
                DrawText(line, box_x + ui_px(10), box_y + ui_px(39), ui_clamp_px(ICON_SIZE_SMALL, ICON_SIZE_SMALL_MIN, ICON_SIZE_SMALL_MAX), c_text);
                locale_format(line, sizeof(line), "results_avg", rounds > 0 ? total / rounds : 0);
                if(view_width < 420 && MeasureText(line, ui_clamp_px(ICON_SIZE_SMALL, ICON_SIZE_SMALL_MIN, ICON_SIZE_SMALL_MAX)) > box_w - ui_px(20))
                    snprintf(line, sizeof(line), "%ds", rounds > 0 ? total / rounds : 0);
                DrawText(line, box_x + ui_px(10), box_y + ui_px(68), ui_clamp_px(ICON_SIZE_SMALL, ICON_SIZE_SMALL_MIN, ICON_SIZE_SMALL_MAX), c_text);
            }

            DrawText(locale_get("round_times_title"), box_x, ui_px(188), ui_clamp_px(14, 12, 16), ui_darken(c_text, 20));
            for(int i = 0; i < rounds; i++) {
                char row[48];
                int seconds = round_times[i];
                locale_format(row, sizeof(row), "round_result_label", i + 1, seconds);
                DrawRectangle(box_x, row_y - 1, box_w, row_h, ui_darken(c_bg, 4));
                DrawLine(box_x, row_y + row_h - 2, box_x + box_w, row_y + row_h - 2, ui_darken(c_bg, 26));
                DrawText(row, box_x + ui_px(10), row_y + ui_px(5), ui_clamp_px(14, 12, 16), c_text);
                row_y += row_h;
            }

            if (ui_draw_text_btn(app, center_x - box_w / 4, action_y, locale_get("discard_button"), &discard_hover)) {
                discard_saved_results(app);
                inbe_app_init(app);
            }
            if (ui_draw_text_btn(app, center_x + box_w / 4, action_y, locale_get("save_results_button"), &save_hover)) {
                if(ensure_results_saved(app))
                    inbe_app_init(app);
            }
            if(discard_hover || save_hover)
                hover = 1;
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

    /* Update DPI cache */
    dpi_update(view_width, view_height);

    ui_init(view_width, view_height, dpi_ui_scale());
#if defined(LOTUS_BUILD)
    sync_lotus_settings(app);
#endif
    update_circle_bounds_for_view(&app->inbe, ui_clamp_px(SETTINGS_TITLE_H, 48, 60),
                                  ui_clamp_px(TAB_BAR_H, 54, 66) + ui_px(80));

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
    dpi_init();
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
    SafeUnloadTexture(app->pencil_icon);
    SafeUnloadTexture(app->save_icon);
    SafeUnloadTexture(app->telegram_icon);
    SafeUnloadTexture(app->monero_icon);
    SafeUnloadTexture(app->angel_image);
    SafeUnloadTexture(app->begin_image);

    /* Cleanup tutorial text layouts */
    if(app->tutorial_layouts_initialized) {
        for(int i = 0; i < 5; i++) {
            ui_text_layout_free(app->tutorial_layouts[i]);
            free(app->tutorial_layouts[i]);
        }
    }

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
