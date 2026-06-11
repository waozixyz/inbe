#include "app.h"
#include "data.h"
#include "locale.h"
#include "tabs/language_tab.h"
#include "tabs/history_tab.h"
#include "tabs/manual_tab.h"
#include "tabs/settings_tab.h"
#include "app_session.h"
#include "theme.h"
#include "theme_meta.h"
#if defined(LOTUS_BUILD)
#include "lotus_settings.h"
#endif
#include "version.h"
#include "flint_ui.h"
#include "flint_dpi.h"

#if !defined(LOTUS_BUILD)
#define RINI_IMPLEMENTATION
#endif
#include "../vendor/rini/src/rini.h"

#include <dirent.h>
#include <limits.h>
#include <stdint.h>
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

#define LOCALE_FONT_PNG "assets/fonts/locales.png"
#define LOCALE_FONT_DAT "assets/fonts/locales.dat"
#define LOCALE_FONT_BASE_SIZE 16

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
    app->settings_category = -1;
    app->settings_sub_tab = 0;
    app->settings_scroll = 0;
    app->inbe.screen = InbeScreenSettings;
}

static UITab g_tabs[] = {
    {NULL, {0}, UI_ICON_TYPE_STAT, history_tab_on_click, NULL},
    {NULL, {0}, UI_ICON_TYPE_MANUAL, on_manual_tab_click, NULL},
    {NULL, {0}, UI_ICON_TYPE_GEAR, on_settings_tab_click, NULL}
};

static UITabBar g_tab_bar = {g_tabs, 3};

static int
load_locale_font(InbeApp *app)
{
    Font font;
    Image white;

    if(app == NULL)
        return 0;

    font = flint_text_load_chopped_font(LOCALE_FONT_PNG, LOCALE_FONT_DAT, LOCALE_FONT_BASE_SIZE);
    if(font.texture.id == 0)
        return 0;

    white = GenImageColor(1, 1, WHITE);
    app->font_shapes_texture = LoadTextureFromImage(white);
    UnloadImage(white);
    if(app->font_shapes_texture.id == 0) {
        flint_text_unload_font(&font);
        return 0;
    }
    SetTextureFilter(app->font_shapes_texture, TEXTURE_FILTER_POINT);

    // Store the locale font in the app for use in text rendering
    app->locale_font = font;
    flint_text_set_font(font);
    SetShapesTexture(app->font_shapes_texture, (Rectangle){0, 0, 1, 1});
    return 1;
}

static void
unload_locale_font(InbeApp *app)
{
    if(app == NULL)
        return;

    flint_text_set_font((Font){0});
    flint_text_unload_font(&app->locale_font);
}

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
register_all_themes(void)
{
    if(DirectoryExists("vendor/flint/themes"))
        flint_theme_register_defaults("vendor/flint/themes");
    else
        flint_theme_register_defaults("themes");
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

void
apply_settings(Inbe *inbe, int speed, int max_rounds, int max_breaths, int pause_seconds)
{
    speed = clampi(speed, SETTINGS_SPEED_MIN, SETTINGS_SPEED_MAX);
    inbe->speed_level = speed;
    inbe->breath_half_ticks = inbe_breath_half_ticks_for_speed(speed);
    inbe->progressive_start_speed = clampi(inbe->progressive_start_speed, SETTINGS_SPEED_MIN, speed);
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
    flint_centered_column(CONTENT_MAX_W, CONTENT_SIDE_PAD, NULL, &content_w);
    update_preview_bounds(&app->settings_preview, content_w, flint_px(132));
    apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
    session_reset_round_breathe(&app->settings_preview);
}

void
save_settings(InbeApp *app)
{
    char text[1200];
    char settings_path[FS_PATH_MAX];
    inbe_settings_path(settings_path, sizeof(settings_path));
#ifdef __ANDROID__
    snprintf(text, sizeof(text),
             "speed %d\nmax_rounds %d\nmax_breaths %d\npause_seconds %d\nsound_volume %d\ntutorial_seen %d\ntheme %d\ndark_mode %d\nfullscreen %d\non_screen_keyboard %d\nprogressive_speed %d\nprogressive_start_speed %d\nadvanced_session_controls %d\nhold_display_mode %d\nplay_in_background %d\nlanguage %s\n",
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
             app->inbe.progressive_start_speed,
             app->advanced_session_controls ? 1 : 0,
             app->hold_display_mode,
             app->inbe.play_in_background,
             (app->language_selected && app->language[0] != '\0')
                 ? app->language
                 : "");
#else
    snprintf(text, sizeof(text),
             "speed %d\nmax_rounds %d\nmax_breaths %d\npause_seconds %d\nsound_volume %d\ntutorial_seen %d\ntheme %d\ndark_mode %d\nfullscreen %d\non_screen_keyboard %d\nprogressive_speed %d\nprogressive_start_speed %d\nadvanced_session_controls %d\nhold_display_mode %d\nlanguage %s\n",
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
             app->inbe.progressive_start_speed,
             app->advanced_session_controls ? 1 : 0,
             app->hold_display_mode,
             (app->language_selected && app->language[0] != '\0')
                 ? app->language
                 : "");
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

    int speed = rini_get_value_fallback(settings, "speed", DefaultSpeedLevel);
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
    app->inbe.progressive_start_speed = clampi(rini_get_value_fallback(settings, "progressive_start_speed", DefaultProgressiveStartSpeed),
                                               SETTINGS_SPEED_MIN, SETTINGS_SPEED_MAX);
    app->advanced_session_controls = rini_get_value_fallback(settings, "advanced_session_controls", 0) != 0;
    app->hold_display_mode = clampi(rini_get_value_fallback(settings, "hold_display_mode", HOLD_DISPLAY_CIRCLE),
                                    HOLD_DISPLAY_CIRCLE, HOLD_DISPLAY_STOPWATCH);
    app->language_needs_save = 0;
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
            app->language_selected = app->tutorial_seen ? 1 : 0;
            app->language_needs_save = app->language_selected;
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
    app->backgrounded = 0;

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
    char icon_name[64];
    char *ext;

    snprintf(icon_name, sizeof(icon_name), "%s", name);
    ext = strrchr(icon_name, '.');
    if(ext != NULL && strcmp(ext, ".png") == 0)
        *ext = '\0';

    return flint_load_icon_texture_by_name(icon_name);
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

void
inbe_app_init(void *vapp) {
    InbeApp *app = vapp;
    if(app == 0)
        return;

    // Initialize locale_font to empty
    app->locale_font = (Font){0};

#ifdef __ANDROID__
    if (app->inbe.screen == InbeScreenSession) {
        android_allow_screen_off();
    }
    if (app->inbe.play_in_background) {
        android_timer_stop();
        android_wakelock_release();
    }
#endif

#if defined(PLATFORM_WEB)
    init_web_storage();
#endif
    locale_init();
    if(!load_locale_font(app)) {
        TraceLog(LOG_WARNING, "FONT: Failed to load chopped locale font -> using built-in default");
    }
    refresh_tab_labels();
    load_config();

    view_width = config.width > 0 ? config.width : INBE_DEFAULT_WIDTH;
    view_height = config.height > 0 ? config.height : INBE_DEFAULT_HEIGHT;
    flint_dpi_update(view_width, view_height);
    ui_init(view_width, view_height, flint_dpi_scale());

    inbeinit(&app->inbe);
    session_update_circle_bounds_for_view(&app->inbe, flint_px(48),
                                  flint_px(56) + flint_px(80));
    load_settings(app);
    if(app->language_needs_save) {
        save_settings(app);
        app->language_needs_save = 0;
    }
    session_update_circle_bounds_for_view(&app->inbe, flint_px(48),
                                  flint_px(56) + 80);
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
    app->settings_category = -1;
    app->settings_sub_tab = 0;
    app->manual_scroll = 0;
    app->manual_drag_scrollbar = 0;
    app->manual_drag_content = 0;
    app->manual_drag_content_y = 0;
    app->tutorial_step = 0;
    history_tab_reset(app);
    app->session_paused = 0;
    app->backgrounded = 0;
    app->results_saved = 0;
    app->results_path[0] = '\0';
    update_session_sounds(app);
    reset_settings_preview(app);
    inbeinit(&app->start_speed_preview);
    app->start_speed_preview_speed = 0;

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
    if(app->discord_icon.id == 0) {
        app->discord_icon = load_icon_texture("discord.png");
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
    if(app->globe_icon.id == 0) {
        app->globe_icon = load_icon_texture("globe.png");
    }
    if(app->monero_icon.id == 0) {
        app->monero_icon = load_icon_texture("monero.png");
    }
    if(app->sound0_icon.id == 0) {
        app->sound0_icon = load_icon_texture("sound0.png");
    }
    if(app->sound1_icon.id == 0) {
        app->sound1_icon = load_icon_texture("sound1.png");
    }
    if(app->sound2_icon.id == 0) {
        app->sound2_icon = load_icon_texture("sound2.png");
    }
    if(app->sound3_icon.id == 0) {
        app->sound3_icon = load_icon_texture("sound3.png");
    }
    app->volume_popup_active = 0;

    ui_set_icons(app->gear_icon, app->x_icon);

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
        settings_tab_clear_status();
        /* Hierarchical navigation: back button goes up one level */
        if(app->settings_category != -1) {
            /* In a sub-tab, go back to category selection */
            app->settings_category = -1;
            app->settings_sub_tab = 0;
            app->settings_scroll = 0;
        } else {
            /* At category selection, go to homepage */
            app->inbe.screen = InbeScreenStart;
            app->settings_scroll = 0;
        }
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
        session_ensure_results_saved(app);
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
        session_update_circle_bounds_for_view(&app->inbe, flint_px(48),
                                      flint_px(56) + 96);
    } else if(app->inbe.screen == InbeScreenSession) {
        session_update_circle_bounds_for_view(&app->inbe, 0, 84);
    }

    if(app->inbe.screen != InbeScreenResults)
        session_draw_inbe(app, center_x, center_y);
    int title_font = flint_px(32);
    int title_w = 30;


    switch (app->inbe.screen) {
    case InbeScreenStart:
        title_w = flint_text_measure(config.title, title_font);
        flint_text_draw(config.title, center_x - title_w / 2, flint_px(20), title_font, c_text);

        {
            int play_y = center_y + (int)(app->inbe.rmax * flint_dpi_scale() + 0.5f) + flint_px(20);
            int play_limit = view_height - flint_px(56) - flint_px(48);
            if(play_y > play_limit)
                play_y = play_limit;
            if (ui_draw_text_btn(center_x, play_y, locale_get("play_button"), &hover)) {
            session_start(app);
            }
        }
        ui_draw_tab_bar(g_tab_bar.tabs, g_tab_bar.count);
        break;

    case InbeScreenSession:
        session_update_screen(app, center_x, center_y, &hover);
        break;

    case InbeScreenResults:
        session_draw_results_screen(app, center_x, center_y, &hover);
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
    flint_dpi_update(view_width, view_height);
    flint_set_view_size(view_width, view_height);

    ui_init(view_width, view_height, flint_dpi_scale());
#if defined(LOTUS_BUILD)
    sync_lotus_settings(app);
#endif
    session_update_circle_bounds_for_view(&app->inbe, flint_px(48),
                                  flint_px(56) + flint_px(80));

    app->cursor_clickable = 0;
    app->camera.zoom = 1.0f;
    app->camera.offset.x = viewport.x;
    app->camera.offset.y = viewport.y;
    ui_set_frame(app->camera);
    ui_set_cursor_clickable(&app->cursor_clickable);

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
    flint_dpi_init();
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
    SafeUnloadTexture(app->discord_icon);
    SafeUnloadTexture(app->telegram_icon);
    SafeUnloadTexture(app->monero_icon);
    SafeUnloadTexture(app->sound0_icon);
    SafeUnloadTexture(app->sound1_icon);
    SafeUnloadTexture(app->sound2_icon);
    SafeUnloadTexture(app->sound3_icon);
    SafeUnloadTexture(app->angel_image);
    SafeUnloadTexture(app->begin_image);
    SafeUnloadTexture(app->font_shapes_texture);
    unload_locale_font(app);

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
