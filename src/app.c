#include "app.h"
#include "data.h"
#include "locale.h"
#include "tabs/language_tab.h"
#include "tabs/manual_tab.h"
#include "tabs/settings_tab.h"
#include "app_session.h"
#include "app_preferences.h"
#include "meditation_music.h"
#include "storage.h"
#include "theme.h"
#include "theme_meta.h"
#if defined(LOTUS_BUILD)
#include "lotus_settings.h"
#endif
#include "version.h"
#include "flint_ui.h"
#include "flint_dpi.h"
#include "flint_text.h"
#include "flint_embedded_assets.h"

#if !defined(LOTUS_BUILD)
#define RINI_IMPLEMENTATION
#endif
#include "../vendor/rini/src/rini.h"

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
    .title = "Inner Breeze",
    .width = INBE_DEFAULT_WIDTH,
    .height = INBE_DEFAULT_HEIGHT,
    .loaded = 0,
    .title_custom = 1
};

int view_width = INBE_DEFAULT_WIDTH;
int view_height = INBE_DEFAULT_HEIGHT;
Color c_text, c_bg, c_surface, c_circle, c_button, c_button_hover, c_icon;

#define LOCALE_FONT_PNG "assets/fonts/locales.png"

static void habit_edit_commit(InbeApp *app);
static void habit_edit_cancel(InbeApp *app);
static void habit_session_cancel_edit(InbeApp *app);
static void habits_sync_topic_theme_colors(InbeApp *app, int sync_topic, int save_now);
static void habits_sync_all_topic_theme_colors(InbeApp *app, int save_now);
#define LOCALE_FONT_DAT "assets/fonts/locales.dat"
#define LOCALE_FONT_BASE_SIZE 16

/* Forward declarations for tab callbacks */
void reset_settings_preview(InbeApp *app);

static void
app_open_main_tab(InbeApp *app, int main_tab, int persist)
{
    if(app == NULL)
        return;

    app->main_tab = clampi(main_tab, APP_MAIN_TAB_HABITS, APP_MAIN_TAB_PRACTICE);
    app->inbe.screen = app->main_tab == APP_MAIN_TAB_HABITS
                           ? InbeScreenHabits
                           : InbeScreenStart;
    if(persist)
        save_settings(app);
}

enum {
    APP_BOTTOM_TAB_NONE = -1,
    APP_BOTTOM_TAB_HABITS = 0,
    APP_BOTTOM_TAB_PRACTICE = 1,
    APP_BOTTOM_TAB_SETTINGS = 2,
    APP_SETTINGS_SAVE_DELAY_TICKS = 30
};

static void
app_schedule_settings_save(InbeApp *app)
{
    if(app == NULL)
        return;
    app->settings_dirty = 1;
    app->settings_save_delay_ticks = APP_SETTINGS_SAVE_DELAY_TICKS;
}

static void
app_request_bottom_tab(InbeApp *app, int bottom_tab)
{
    if(app == NULL)
        return;
    app->pending_bottom_tab = bottom_tab;
}

static void
app_apply_bottom_tab(InbeApp *app, int bottom_tab)
{
    if(app == NULL)
        return;

    switch(bottom_tab) {
    case APP_BOTTOM_TAB_HABITS:
        app_open_main_tab(app, APP_MAIN_TAB_HABITS, 0);
        app_schedule_settings_save(app);
        break;
    case APP_BOTTOM_TAB_PRACTICE:
        app_open_main_tab(app, APP_MAIN_TAB_PRACTICE, 0);
        app_schedule_settings_save(app);
        break;
    case APP_BOTTOM_TAB_SETTINGS:
        reset_settings_preview(app);
        app->settings_category = -1;
        app->settings_sub_tab = 0;
        app->app_settings_tab = APP_SETTINGS_TAB_APP;
        app->settings_from_exercise_selector = 0;
        app->settings_scroll = 0;
        app->inbe.screen = InbeScreenSettings;
        app_schedule_settings_save(app);
        break;
    default:
        break;
    }
}

static void
app_apply_pending_bottom_tab(InbeApp *app)
{
    int pending;

    if(app == NULL || app->pending_bottom_tab == APP_BOTTOM_TAB_NONE)
        return;

    pending = app->pending_bottom_tab;
    app->pending_bottom_tab = APP_BOTTOM_TAB_NONE;
    app_apply_bottom_tab(app, pending);
}

static void
app_flush_deferred_settings(InbeApp *app)
{
    if(app == NULL || app->settings_save_delay_ticks <= 0)
        return;

    app->settings_save_delay_ticks--;
    if(app->settings_save_delay_ticks <= 0 && app->settings_dirty)
        save_settings(app);
}

static int
app_should_draw_bottom_nav(const InbeApp *app)
{
    if(app == NULL || app->modal.active)
        return 0;

    switch(app->inbe.screen) {
    case InbeScreenStart:
    case InbeScreenSettings:
    case InbeScreenHabits:
    case InbeScreenHabitManager:
    case InbeScreenHabitEdit:
    case InbeScreenPracticeConfig:
        return 1;
    case InbeScreenHabitSessionEdit:
        return !app->habit_session_edit_active;
    default:
        return 0;
    }
}

static void app_draw_bottom_nav(InbeApp *app);

static int
exercise_manual_seen_bit(int exercise_type)
{
    if(exercise_type < 0 || exercise_type >= EXERCISE_COUNT)
        return 0;
    return 1 << exercise_type;
}

int
exercise_manual_seen(InbeApp *app, int exercise_type)
{
    int bit = exercise_manual_seen_bit(exercise_type);
    if(app == NULL || bit == 0)
        return 1;
    return (app->exercise_manual_seen_mask & bit) != 0;
}

void
mark_exercise_manual_seen(InbeApp *app, int exercise_type)
{
    int bit = exercise_manual_seen_bit(exercise_type);
    if(app == NULL || bit == 0)
        return;
    if((app->exercise_manual_seen_mask & bit) == 0) {
        app->exercise_manual_seen_mask |= bit;
        save_settings(app);
    }
}

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
    app->theme_mode = lotus->dark_mode ? APP_THEME_DARK : APP_THEME_LIGHT;
    app->dark_mode = app_effective_dark_mode(app);
    snprintf(app->language, sizeof(app->language), "%.*s",
             (int)sizeof(app->language) - 1, lotus->language);
    app->language_selected = 1;
    if(!locale_set(app->language)) {
        snprintf(app->language, sizeof(app->language), "%s", "en");
        locale_set(app->language);
    }

    app_refresh_theme(app);
    refresh_locale_dependent_text(app);
    app->lotus_settings_version = version;
}
#endif

/* ================================================================
 * TAB BAR DEFINITIONS
 * ================================================================ */

static void on_habits_tab_click(void *user_data) {
    InbeApp *app = user_data;
    app_request_bottom_tab(app, APP_BOTTOM_TAB_HABITS);
}

static void on_practice_tab_click(void *user_data) {
    InbeApp *app = user_data;
    app_request_bottom_tab(app, APP_BOTTOM_TAB_PRACTICE);
}

static void on_settings_tab_click(void *user_data) {
    InbeApp *app = user_data;
    app_request_bottom_tab(app, APP_BOTTOM_TAB_SETTINGS);
}

static UITab g_tabs[] = {
    {NULL, {0}, UI_ICON_TYPE_HOME, on_habits_tab_click, NULL},
    {NULL, {0}, UI_ICON_TYPE_AMEN, on_practice_tab_click, NULL},
    {NULL, {0}, UI_ICON_TYPE_GEAR, on_settings_tab_click, NULL}
};

static UITabBar g_tab_bar = {g_tabs, 3};

static void
app_draw_bottom_nav(InbeApp *app)
{
    if(app_should_draw_bottom_nav(app))
        ui_draw_tab_bar(g_tab_bar.tabs, g_tab_bar.count);
}

#define PRACTICE_CATEGORY_TAB_H 40
#define PRACTICE_CATEGORY_TOAST_TICKS 120
#define PRACTICE_CONFIG_TAB_LIST (-1)

static const char *g_practice_category_labels[PRACTICE_CATEGORY_COUNT] = {
    "Mind",
    "Yoga",
    "Fitness"
};

static const int g_practice_category_default_themes[PRACTICE_CATEGORY_COUNT] = {
    ThemeSky,
    ThemeSunset,
    ThemeCherry
};

static int
load_locale_font(InbeApp *app)
{
    Font font;
    Image white;
    const FlintEmbeddedAsset *png;
    const FlintEmbeddedAsset *dat;

    if(app == NULL)
        return 0;

    png = flint_embedded_asset(LOCALE_FONT_PNG);
    dat = flint_embedded_asset(LOCALE_FONT_DAT);
    if(png == NULL || dat == NULL)
        return 0;

    font = flint_text_load_chopped_font_from_memory(png->data, png->size, dat->data, dat->size,
                                                    LOCALE_FONT_BASE_SIZE);
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
    g_tabs[0].label = locale_get("tab_habits");
    g_tabs[1].label = locale_get("tab_practice");
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
            if(Module.inbeSyncfsTimer)
                clearTimeout(Module.inbeSyncfsTimer);
            Module.inbeSyncfsTimer = setTimeout(function() {
                Module.inbeSyncfsTimer = 0;
                try {
                    FS.syncfs(false, function(err) {
                        if(err) console.error("IDBFS save failed:", err);
                        else console.log("IDBFS synced");
                    });
                } catch(e) {
                    console.error("IDBFS sync error:", e);
                }
            }, 250);
        }
    });
}
#endif

static void
load_config(void)
{
    if(config.loaded)
        return;

    if(config.title[0] == '\0') {
        snprintf(config.title, sizeof(config.title), "%s", locale_get("app_title"));
        config.title_custom = 0;
    }

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
    if(app == NULL)
        return;

    inbe_storage_settings_begin_write();
    inbe_storage_set_setting_int("speed", app->inbe.speed_level);
    inbe_storage_set_setting_int("max_rounds", app->inbe.max_rounds);
    inbe_storage_set_setting_int("max_breaths", int_from_count(app->inbe.maxbreaths));
    inbe_storage_set_setting_int("pause_seconds", app->inbe.pause_seconds);
    inbe_storage_set_setting_int("sound_volume", app->sound_volume);
    inbe_storage_set_setting_int("tutorial_seen", app->tutorial_seen ? 1 : 0);
    inbe_storage_set_setting_int("exercise_manual_seen_mask", app->exercise_manual_seen_mask);
    inbe_storage_set_setting_int("theme", app->theme_id);
    inbe_storage_set_setting_int("dark_mode", app->dark_mode);
    inbe_storage_set_setting_int("theme_mode", app->theme_mode);
    inbe_storage_set_setting_int("orientation_mode", app->orientation_mode);
    inbe_storage_set_setting_int("main_tab", app->main_tab);
    inbe_storage_set_setting_int("habits_view_mode", app->habits_view_mode);
    inbe_storage_set_setting_int("fullscreen", app->fullscreen_enabled ? 1 : 0);
    inbe_storage_set_setting_int("on_screen_keyboard", app->on_screen_keyboard_enabled ? 1 : 0);
    inbe_storage_set_setting_int("progressive_speed", app->inbe.progressive_speed);
    inbe_storage_set_setting_int("progressive_start_speed", app->inbe.progressive_start_speed);
    inbe_storage_set_setting_int("advanced_session_controls", app->advanced_session_controls ? 1 : 0);
    inbe_storage_set_setting_int("hold_display_mode", app->hold_display_mode);
    inbe_storage_set_setting_int("exercise_type", app->exercise_type);
    inbe_storage_set_setting_int("meditation_music_enabled", app->meditation_music_enabled ? 1 : 0);
    inbe_storage_set_setting_int("meditation_music_shuffle", app->meditation_music_shuffle ? 1 : 0);
    inbe_storage_set_setting_int("meditation_music_track", app->meditation_music_track);
    inbe_storage_set_setting_int("play_in_background", app->inbe.play_in_background);
    inbe_storage_set_setting_text("language",
                                  (app->language_selected && app->language[0] != '\0') ? app->language : "");
    inbe_storage_set_setting_int("practice_tab_enabled_mask",
                                 (app->practice_tab_enabled[PRACTICE_CATEGORY_MIND] ? 1 : 0) |
                                     (app->practice_tab_enabled[PRACTICE_CATEGORY_YOGA] ? 2 : 0) |
                                     (app->practice_tab_enabled[PRACTICE_CATEGORY_FITNESS] ? 4 : 0));
    inbe_storage_set_setting_int("practice_tab_mind_theme", app->practice_tab_theme[PRACTICE_CATEGORY_MIND]);
    inbe_storage_set_setting_int("practice_tab_yoga_theme", app->practice_tab_theme[PRACTICE_CATEGORY_YOGA]);
    inbe_storage_set_setting_int("practice_tab_fitness_theme", app->practice_tab_theme[PRACTICE_CATEGORY_FITNESS]);
    inbe_storage_set_setting_int("practice_category_tab", app->practice_category_tab);
    inbe_storage_settings_end_write();
#if defined(PLATFORM_WEB)
    sync_web_storage();
#endif
    app->settings_dirty = 0;
    app->settings_save_delay_ticks = 0;
}

static void
load_settings(InbeApp *app)
{
    int settings_missing = inbe_storage_settings_empty();
    int speed = inbe_storage_get_setting_int("speed", DefaultSpeedLevel);
    int max_rounds = inbe_storage_get_setting_int("max_rounds", DefaultMaxRounds);
    int max_breaths = inbe_storage_get_setting_int("max_breaths", DefaultMaxBreaths);
    int pause_seconds = inbe_storage_get_setting_int("pause_seconds", DefaultPauseSeconds);
    int sound_volume = inbe_storage_get_setting_int("sound_volume", 100);
    int manual_seen_mask;
    int practice_enabled_mask;

    app->tutorial_seen = inbe_storage_get_setting_int("tutorial_seen", 0) != 0;
    manual_seen_mask = inbe_storage_get_setting_int("exercise_manual_seen_mask", -1);
    if(manual_seen_mask < 0)
        manual_seen_mask = app->tutorial_seen ? exercise_manual_seen_bit(EXERCISE_WIM_HOF) : 0;
    app->exercise_manual_seen_mask = manual_seen_mask & ((1 << EXERCISE_COUNT) - 1);
    app->theme_id = clampi(inbe_storage_get_setting_int("theme", 0), 0, THEME_COUNT - 1);
    app->theme_mode = clampi(inbe_storage_get_setting_int("theme_mode", APP_THEME_SYSTEM),
                             APP_THEME_SYSTEM, APP_THEME_DARK);
    app->dark_mode = inbe_storage_get_setting_int("dark_mode", 0) != 0;
    app->orientation_mode = clampi(inbe_storage_get_setting_int("orientation_mode", APP_ORIENTATION_SYSTEM),
                                   APP_ORIENTATION_SYSTEM, APP_ORIENTATION_SENSOR);
    app->main_tab = clampi(inbe_storage_get_setting_int("main_tab", APP_MAIN_TAB_PRACTICE),
                           APP_MAIN_TAB_HABITS, APP_MAIN_TAB_PRACTICE);
    app->habits_view_mode = clampi(inbe_storage_get_setting_int("habits_view_mode", HABITS_VIEW_CALENDAR),
                                   HABITS_VIEW_CALENDAR, HABITS_VIEW_LINKED);
    app->fullscreen_enabled = inbe_storage_get_setting_int("fullscreen", 0) != 0;
#ifdef __ANDROID__
    app->on_screen_keyboard_enabled = inbe_storage_get_setting_int("on_screen_keyboard", 1) != 0;
#else
    app->on_screen_keyboard_enabled = inbe_storage_get_setting_int("on_screen_keyboard", 0) != 0;
#endif
    app->sound_volume = clampi(sound_volume, SETTINGS_VOLUME_MIN, SETTINGS_VOLUME_MAX);
    app->inbe.progressive_speed = inbe_storage_get_setting_int("progressive_speed", 1) != 0;
    app->inbe.progressive_start_speed = clampi(inbe_storage_get_setting_int("progressive_start_speed", DefaultProgressiveStartSpeed),
                                               SETTINGS_SPEED_MIN, SETTINGS_SPEED_MAX);
    app->advanced_session_controls = inbe_storage_get_setting_int("advanced_session_controls", 0) != 0;
    app->hold_display_mode = clampi(inbe_storage_get_setting_int("hold_display_mode", HOLD_DISPLAY_CIRCLE),
                                    HOLD_DISPLAY_CIRCLE, HOLD_DISPLAY_STOPWATCH);
    app->exercise_type = clampi(inbe_storage_get_setting_int("exercise_type", EXERCISE_WIM_HOF),
                                EXERCISE_WIM_HOF, EXERCISE_COUNT - 1);
    app->meditation_music_enabled = inbe_storage_get_setting_int("meditation_music_enabled", 1) != 0;
    app->meditation_music_shuffle = inbe_storage_get_setting_int("meditation_music_shuffle", 0) != 0;
    app->meditation_music_track = clampi(inbe_storage_get_setting_int("meditation_music_track", 0),
                                         0, MEDITATION_MUSIC_TRACK_COUNT - 1);
    practice_enabled_mask = inbe_storage_get_setting_int("practice_tab_enabled_mask", 7);
    if((practice_enabled_mask & 7) == 0)
        practice_enabled_mask = 1;
    for(int i = 0; i < PRACTICE_CATEGORY_COUNT; i++) {
        app->practice_tab_enabled[i] = (practice_enabled_mask & (1 << i)) != 0;
        app->practice_tab_theme[i] = g_practice_category_default_themes[i];
    }
    app->practice_tab_theme[PRACTICE_CATEGORY_MIND] =
        clampi(inbe_storage_get_setting_int("practice_tab_mind_theme", ThemeSky), 0, THEME_COUNT - 1);
    app->practice_tab_theme[PRACTICE_CATEGORY_YOGA] =
        clampi(inbe_storage_get_setting_int("practice_tab_yoga_theme", ThemeSunset), 0, THEME_COUNT - 1);
    app->practice_tab_theme[PRACTICE_CATEGORY_FITNESS] =
        clampi(inbe_storage_get_setting_int("practice_tab_fitness_theme", ThemeCherry), 0, THEME_COUNT - 1);
    app->practice_tab_enabled[PRACTICE_CATEGORY_YOGA] = 0;
    app->practice_tab_enabled[PRACTICE_CATEGORY_FITNESS] = 0;
    app->practice_category_tab =
        clampi(inbe_storage_get_setting_int("practice_category_tab", PRACTICE_CATEGORY_MIND),
               0, PRACTICE_CATEGORY_COUNT - 1);
    if(!app->practice_tab_enabled[app->practice_category_tab])
        app->practice_category_tab = PRACTICE_CATEGORY_MIND;
    app->theme_id = clampi(app->practice_tab_theme[app->practice_category_tab], 0, THEME_COUNT - 1);
    app->language_needs_save = 0;
    {
        const char *language = inbe_storage_get_setting_text("language");
        if(language != NULL && language[0] != '\0') {
            snprintf(app->language, sizeof(app->language), "%s", language);
            app->language_selected = 1;
            if(!locale_set(app->language)) {
                snprintf(app->language, sizeof(app->language), "%s", "en");
                locale_set(app->language);
            }
        } else {
            snprintf(app->language, sizeof(app->language), "%s", "en");
            app->language_selected = settings_missing ? 0 : 1;
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
    app->inbe.play_in_background = inbe_storage_get_setting_int("play_in_background",
        1  // Default to enabled on Android
    );
    TraceLog(LOG_INFO, "INBE: Loaded play_in_background setting = %d", app->inbe.play_in_background);
#else
    app->inbe.play_in_background = 0;
#endif
    app->backgrounded = 0;

    app_device_preferences_init(app);
    apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
    refresh_locale_dependent_text(app);
    if(settings_missing)
        save_settings(app);
}

static Texture2D
load_pixel_texture_from_asset(const char *path)
{
    const FlintEmbeddedAsset *asset = flint_embedded_asset(path);
    Image image;
    Texture2D texture = {0};

    if(asset == NULL || asset->data == NULL || asset->size == 0)
        return texture;

    image = LoadImageFromMemory(flint_embedded_asset_extension(path), asset->data, (int)asset->size);
    if(image.data == NULL)
        return texture;

#if defined(_WIN32)
    {
        int pot_w = 1;
        int pot_h = 1;
        while(pot_w < image.width)
            pot_w <<= 1;
        while(pot_h < image.height)
            pot_h <<= 1;
        if(pot_w != image.width || pot_h != image.height)
            ImageResizeCanvas(&image, pot_w, pot_h, 0, 0, BLANK);
    }
#endif

    texture = LoadTextureFromImage(image);
    UnloadImage(image);
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
    return load_pixel_texture_from_asset(path);
}

static Sound
load_sound_asset(const char *name)
{
    char path[96];
    const FlintEmbeddedAsset *asset;
    Wave wave;
    Sound sound = {0};

    snprintf(path, sizeof(path), "assets/sounds/%s", name);
    asset = flint_embedded_asset(path);
    if(asset == NULL || asset->data == NULL || asset->size == 0)
        return sound;

    wave = LoadWaveFromMemory(flint_embedded_asset_extension(path), asset->data, (int)asset->size);
    if(wave.data == NULL)
        return sound;

    sound = LoadSoundFromWave(wave);
    UnloadWave(wave);
    return sound;
}

void
app_play_sound(InbeApp *app, Sound sound, float scale)
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
    data_init();
    load_settings(app);
    if(app->language_needs_save) {
        save_settings(app);
        app->language_needs_save = 0;
    }
    session_update_circle_bounds_for_view(&app->inbe, flint_px(48),
                                  flint_px(56) + 80);
    inbe_habits_init(&app->habits);
    habits_sync_all_topic_theme_colors(app, 1);
    app->habits_list_scroll = 0;
    app->habits_list_expanded_year = 0;
    app->habits_list_expanded_month = 0;
    app->habits_list_expanded_day = 0;
    app->habits_list_expanded_session = -1;
    app->habit_detail_index = -1;
    app->habit_detail_day = 0;
    app->habit_detail_session_index = -1;
    app->habit_detail_session_path[0] = '\0';
    app->habit_session_edit_scroll = 0;
    app->habit_session_edit_active = 0;
    app->habit_session_edit_kind = 0;
    app->habit_session_edit_round = -1;
    app->habit_session_edit_cursor = 0;
    app->habit_session_edit_path[0] = '\0';
    app->habit_session_edit_text[0] = '\0';
    app->pending_bottom_tab = APP_BOTTOM_TAB_NONE;
    app_open_main_tab(app, app->main_tab, 0);
    init_audio(app);
    meditation_music_init(app);
    app->camera = (Camera2D){0};
    app->cursor_clickable = 0;
    app->cursor_disabled = 0;
    app->play_circle_hover = 0;
    app->play_circle_scale = 1.0f;
    app->settings_scroll = 0;
    app->settings_drag_slider = 0;
    app->settings_drag_scrollbar = 0;
    app->settings_drag_content = 0;
    app->settings_drag_content_y = 0;
    app->settings_dirty = 0;
    app->settings_save_delay_ticks = 0;
    app->settings_category = -1;
    app->settings_sub_tab = 0;
    app->app_settings_tab = APP_SETTINGS_TAB_APP;
    app->settings_from_exercise_selector = 0;
    app->practice_coming_soon_ticks = 0;
    app->practice_config_theme_tab = PRACTICE_CONFIG_TAB_LIST;
    app->habit_edit_active = 0;
    app->habit_edit_is_new = 0;
    app->habit_edit_index = -1;
    app->habit_edit_cursor = 0;
    app->habit_edit_text[0] = '\0';
    app->habit_edit_color = (Color){99, 196, 165, 255};
    app->habit_edit_sync_mode = INBE_HABIT_SYNC_NONE;
    app->habit_edit_sync_topic = INBE_HABIT_TOPIC_MIND;
    app->habit_edit_sync_activity = EXERCISE_WIM_HOF;
    app->manual_scroll = 0;
    app->manual_drag_scrollbar = 0;
    app->manual_drag_content = 0;
    app->manual_drag_content_y = 0;
    app->tutorial_step = 0;
    app->session_paused = 0;
    app->backgrounded = 0;
    app->results_saved = 0;
    app->results_path[0] = '\0';
    update_session_sounds(app);
    reset_settings_preview(app);
    inbeinit(&app->start_speed_preview);
    app->start_speed_preview_speed = 0;

    // Load all icons using a clean array-based approach
    static const char *icon_names[UI_ICON_TYPE_COUNT] = {
        NULL,                 // UI_ICON_TYPE_NONE (no icon)

        // Core UI icons
        "gear.png",            // UI_ICON_TYPE_GEAR
        "x.png",               // UI_ICON_TYPE_X
        "x-red.png",           // UI_ICON_TYPE_X_RED
        "manual.png",          // UI_ICON_TYPE_MANUAL
        "return.png",          // UI_ICON_TYPE_RETURN
        "backward.png",        // UI_ICON_TYPE_BACKWARD
        "forward.png",         // UI_ICON_TYPE_FORWARD
        "play.png",            // UI_ICON_TYPE_PLAY
        "pause.png",           // UI_ICON_TYPE_PAUSE
        "stat.png",            // UI_ICON_TYPE_STAT
        "home.png",            // UI_ICON_TYPE_HOME
        "trash.png",           // UI_ICON_TYPE_TRASH
        "pencil.png",          // UI_ICON_TYPE_PENCIL
        "save.png",            // UI_ICON_TYPE_SAVE
        "plus.png",            // UI_ICON_TYPE_PLUS
        "stack.png",           // UI_ICON_TYPE_STACK

        // Social & payment icons
        "discord.png",         // UI_ICON_TYPE_DISCORD
        "telegram.png",        // UI_ICON_TYPE_TELEGRAM
        "github.png",          // UI_ICON_TYPE_GITHUB
        "globe.png",           // UI_ICON_TYPE_GLOBE
        "stripe.png",          // UI_ICON_TYPE_STRIPE
        "btc.png",             // UI_ICON_TYPE_BTC
        "monero.png",          // UI_ICON_TYPE_MONERO

        // Sound icons
        "sound.png",           // UI_ICON_TYPE_SOUND
        "sound0.png",          // UI_ICON_TYPE_SOUND0
        "sound1.png",          // UI_ICON_TYPE_SOUND1
        "sound2.png",          // UI_ICON_TYPE_SOUND2
        "sound3.png",          // UI_ICON_TYPE_SOUND3
        "mute.png",            // UI_ICON_TYPE_MUTE

        // Habit & practice icons
        "habitmarker.png",     // UI_ICON_TYPE_HABIT
        "amen.png",            // UI_ICON_TYPE_AMEN
        "inbe.png",            // UI_ICON_TYPE_INBE

        // Meditation & theme icons
        "droid.png",           // UI_ICON_TYPE_DROID
        "fdroid.png",          // UI_ICON_TYPE_FDROID
        "lighton.png",         // UI_ICON_TYPE_LIGHTON
        "lightoff.png",        // UI_ICON_TYPE_LIGHTOFF
        "moon.png",            // UI_ICON_TYPE_MOON
        "sun.png",             // UI_ICON_TYPE_SUN
        "jupiter.png",         // UI_ICON_TYPE_JUPITER
        "mars.png",            // UI_ICON_TYPE_MARS
        "mercury.png",         // UI_ICON_TYPE_MERCURY
        "venus.png",           // UI_ICON_TYPE_VENUS
        "saturn.png",          // UI_ICON_TYPE_SATURN

        // Navigation & utility icons
        "link.png",            // UI_ICON_TYPE_LINK
        "edit.png",            // UI_ICON_TYPE_EDIT
        "eye-closed.png",      // UI_ICON_TYPE_EYE_CLOSED
        "check.png",           // UI_ICON_TYPE_CHECK
        "quest.png",           // UI_ICON_TYPE_QUEST
        "routine.png",         // UI_ICON_TYPE_ROUTINE
        "timeline.png",        // UI_ICON_TYPE_TIMELINE
        "todos.png",           // UI_ICON_TYPE_TODOS
        "tile.png",            // UI_ICON_TYPE_TILE
        "tile2.png",           // UI_ICON_TYPE_TILE2
        "tile3.png",           // UI_ICON_TYPE_TILE3
        "tile4.png",           // UI_ICON_TYPE_TILE4
        "text.png",            // UI_ICON_TYPE_TEXT

        // Platform & store icons
        "itch.png",            // UI_ICON_TYPE_ITCH
        "playstore.png",       // UI_ICON_TYPE_PLAYSTORE
        "tux.png",             // UI_ICON_TYPE_TUX
        "win.png",             // UI_ICON_TYPE_WIN
        "uxn.png",             // UI_ICON_TYPE_UXN
        "wasm.png",            // UI_ICON_TYPE_WASM
        "wasm4.png",           // UI_ICON_TYPE_WASM4
        "ray.png",             // UI_ICON_TYPE_RAY
        "rocket.png",          // UI_ICON_TYPE_ROCKET
        "srht.png",            // UI_ICON_TYPE_SRHT
        "tcl.png",             // UI_ICON_TYPE_TCL
    };

    for(int i = 1; i < UI_ICON_TYPE_COUNT; i++) {  // Skip UI_ICON_TYPE_NONE (index 0)
        if(icon_names[i] != NULL && app->icons[i].id == 0) {
            app->icons[i] = load_icon_texture(icon_names[i]);
        }
    }

    /* Update tab bar icons */
    g_tabs[0].icon = app->icons[UI_ICON_TYPE_HABIT];
    g_tabs[0].icon_type = UI_ICON_TYPE_NONE;
    g_tabs[0].user_data = app;
    g_tabs[1].icon = app->icons[UI_ICON_TYPE_AMEN];
    g_tabs[1].icon_type = UI_ICON_TYPE_AMEN;
    g_tabs[1].user_data = app;
    g_tabs[2].icon = app->icons[UI_ICON_TYPE_GEAR];
    g_tabs[2].icon_type = UI_ICON_TYPE_GEAR;
    g_tabs[2].user_data = app;

    app->volume_popup_active = 0;

    ui_set_icons(app->icons[UI_ICON_TYPE_GEAR], app->icons[UI_ICON_TYPE_X]);

    if(app->whm_1_image.id == 0) {
        app->whm_1_image = load_asset_texture("whm/1.jpg");
    }
    if(app->whm_2_image.id == 0) {
        app->whm_2_image = load_asset_texture("whm/2.jpg");
    }
#if !defined(LOTUS_BUILD)
    if(!app->language_selected)
        app->inbe.screen = InbeScreenLanguage;
    else
        app->inbe.screen = InbeScreenStart;
#else
    app->inbe.screen = InbeScreenStart;
#endif

    /* Reset modal state */
    app->modal.active = 0;
    app->modal.type = UIModalNone;
    app->modal.selected_button = 0;
    app->meditation_duration_seconds = 0;
    app->meditation_remaining_seconds = 0;
    app->meditation_frame_ticks = 0;
}

static Texture2D
sound_icon_for_volume(InbeApp *app)
{
    int vol = app->sound_volume;
    if(vol <= 0)
        return app->icons[UI_ICON_TYPE_SOUND0];
    if(vol <= 25)
        return app->icons[UI_ICON_TYPE_SOUND1];
    if(vol <= 75)
        return app->icons[UI_ICON_TYPE_SOUND2];
    return app->icons[UI_ICON_TYPE_SOUND3];
}

static void
meditation_start(InbeApp *app, int seconds)
{
    if(app == NULL)
        return;

    app->meditation_duration_seconds = seconds;
    app->meditation_remaining_seconds = seconds;
    app->meditation_frame_ticks = 0;
    app->session_paused = 0;
    app->volume_popup_active = 0;
    app->modal.active = 0;
    app->modal.type = UIModalNone;
    app->inbe.screen = InbeScreenMeditation;
    app_play_sound(app, app->bell_sound, 1.0f);
    meditation_music_start_session(app);
}

static int
habit_topic_for_activity(int exercise_type)
{
    if(exercise_type == EXERCISE_SUN_SALUTATION)
        return INBE_HABIT_TOPIC_YOGA;
    if(exercise_type == EXERCISE_7_MINUTE_WORKOUT)
        return INBE_HABIT_TOPIC_FITNESS;
    return INBE_HABIT_TOPIC_MIND;
}

void
sync_habits_for_activity(InbeApp *app, int exercise_type)
{
    int topic;
    int today;
    int selected;
    int changed = 0;

    if(app == NULL)
        return;

    topic = habit_topic_for_activity(exercise_type);
    today = inbe_habits_today_index();
    selected = app->habits.selected;
    for(int i = 0; i < app->habits.count; i++) {
        InbeHabit *habit = &app->habits.items[i];
        if(habit->sync_mode == INBE_HABIT_SYNC_TOPIC &&
           habit->sync_topic == topic) {
            if(!inbe_habit_completed_day(habit, today)) {
                inbe_habit_set_day(&app->habits, i, today, 1);
                changed = 1;
            }
        } else if(habit->sync_mode == INBE_HABIT_SYNC_ACTIVITY &&
                  habit->sync_activity == exercise_type) {
            if(!inbe_habit_completed_day(habit, today)) {
                inbe_habit_set_day(&app->habits, i, today, 1);
                changed = 1;
            }
        }
    }
    if(changed) {
        app->habits.selected = selected;
        inbe_habits_save(&app->habits);
    }
}

static void
meditation_finish(InbeApp *app)
{
    if(app == NULL)
        return;

    app_play_sound(app, app->bell_sound, 1.0f);
    app->meditation_duration_seconds = 0;
    app->meditation_remaining_seconds = 0;
    app->meditation_frame_ticks = 0;
    app->session_paused = 0;
    app->volume_popup_active = 0;
    sync_habits_for_activity(app, EXERCISE_MEDITATION);
    meditation_music_stop(app);
    app->inbe.screen = InbeScreenStart;
}

static void
format_meditation_time(char *dst, size_t dst_size, int seconds)
{
    int hours;
    int minutes;
    int secs;

    if(dst == NULL || dst_size == 0)
        return;
    if(seconds < 0)
        seconds = 0;

    hours = seconds / 3600;
    minutes = (seconds / 60) % 60;
    secs = seconds % 60;

    if(hours > 0)
        snprintf(dst, dst_size, "%d:%02d:%02d", hours, minutes, secs);
    else
        snprintf(dst, dst_size, "%d:%02d", minutes, secs);
}

static int
draw_meditation_duration_button(int x, int y, int w, int h, const char *label)
{
    int hover = 0;
    return ui_draw_generic_button(x, y, w, h, label, UI_BUTTON_STYLE_PRIMARY, 0, &hover);
}

static void
draw_meditation_setup_modal(InbeApp *app)
{
    static const int durations[] = {5 * 60, 15 * 60, 30 * 60, 60 * 60, 2 * 60 * 60};
    const char *labels[] = {
        locale_get("duration_5m"),
        locale_get("duration_15m"),
        locale_get("duration_30m"),
        locale_get("duration_1h"),
        locale_get("duration_2h")
    };
    int modal_w = flint_px(320);
    int modal_h = flint_px(236);
    int modal_x;
    int modal_y;
    int title_font = flint_px(18);
    int title_w;
    int btn_h = flint_px(38);
    int gap = flint_px(10);
    int side = flint_px(18);
    int row_y;
    int btn_w;
    int cancel_w = flint_px(120);
    int cancel_x;
    int cancel_y;
    int cancel_hover = 0;

    if(modal_w > view_width - flint_px(24))
        modal_w = view_width - flint_px(24);
    if(modal_h > view_height - flint_px(24))
        modal_h = view_height - flint_px(24);

    modal_x = (view_width - modal_w) / 2;
    modal_y = (view_height - modal_h) / 2;
    btn_w = (modal_w - side * 2 - gap * 2) / 3;
    if(btn_w < flint_px(64))
        btn_w = flint_px(64);

    DrawRectangle(0, 0, view_width, view_height, (Color){0, 0, 0, 180});
    DrawRectangle(modal_x, modal_y, modal_w, modal_h, c_surface);
    ui_draw_bevel(modal_x, modal_y, modal_w, modal_h,
                  flint_lighten(c_surface, 40), flint_darken(c_surface, 40));

    title_w = flint_text_measure(locale_get("meditation_title"), title_font);
    flint_text_draw(locale_get("meditation_title"), modal_x + (modal_w - title_w) / 2,
                    modal_y + flint_px(16), title_font, c_text);

    row_y = modal_y + flint_px(62);
    for(int i = 0; i < 3; i++) {
        int x = modal_x + side + i * (btn_w + gap);
        if(draw_meditation_duration_button(x, row_y, btn_w, btn_h, labels[i]))
            meditation_start(app, durations[i]);
    }

    row_y += btn_h + gap;
    for(int i = 3; i < 5; i++) {
        int two_w = (modal_w - side * 2 - gap) / 2;
        int x = modal_x + side + (i - 3) * (two_w + gap);
        if(draw_meditation_duration_button(x, row_y, two_w, btn_h, labels[i]))
            meditation_start(app, durations[i]);
    }

    cancel_y = modal_y + modal_h - btn_h - flint_px(16);
    cancel_x = modal_x + (modal_w - cancel_w) / 2;
    if(ui_draw_generic_button(cancel_x, cancel_y, cancel_w, btn_h,
                              locale_get("cancel_button"), UI_BUTTON_STYLE_SECONDARY,
                              0, &cancel_hover)) {
        app->modal.active = 0;
        app->modal.type = UIModalNone;
    }
}

static void
draw_meditation_sound_controls(InbeApp *app)
{
    int sound_btn_x = view_width - flint_px(56);
    int sound_btn_y = flint_px(12);
    int sound_btn_size = flint_px(24);
    int sound_btn_padding = flint_px(10);
    int sound_hover = 0;

    if(ui_draw_icon_btn_padded(sound_btn_x, sound_btn_y, sound_btn_size, sound_btn_padding,
                               sound_icon_for_volume(app), &sound_hover)) {
        app->volume_popup_active = !app->volume_popup_active;
    }

    if(app->volume_popup_active) {
        int popup_w = flint_px(44);
        int popup_x = sound_btn_x;
        int popup_y = sound_btn_y + sound_btn_size + sound_btn_padding * 2;
        int popup_h = flint_px(200);
        Vector2 mouse = GetMousePosition();

        if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
           (mouse.x < popup_x || mouse.x > popup_x + popup_w ||
            mouse.y < popup_y || mouse.y > popup_y + popup_h)) {
            app->volume_popup_active = 0;
        }

        DrawRectangle(popup_x, popup_y, popup_w, popup_h, c_surface);
        ui_draw_bevel(popup_x, popup_y, popup_w, popup_h,
                      flint_lighten(c_surface, 40), flint_darken(c_surface, 40));

        if(ui_draw_slider_vertical(501, popup_x + popup_w / 2, popup_y + flint_px(10),
                                   popup_h - flint_px(20), SETTINGS_VOLUME_MIN,
                                   SETTINGS_VOLUME_MAX, &app->sound_volume)) {
            app->settings_dirty = 1;
            save_settings(app);
        }
    }
}

static void
draw_meditation_screen(InbeApp *app, int center_x, int center_y)
{
    char time_text[32];
    int return_hover = 0;
    int font = flint_px(48);
    int max_w;
    int text_w;

    if(ui_draw_icon_btn_padded(flint_px(12), flint_px(12), flint_px(24),
                               flint_px(10), app->icons[UI_ICON_TYPE_RETURN], &return_hover)) {
        meditation_music_stop(app);
        app->meditation_duration_seconds = 0;
        app->meditation_remaining_seconds = 0;
        app->meditation_frame_ticks = 0;
        app->volume_popup_active = 0;
        app->inbe.screen = InbeScreenStart;
        return;
    }

    draw_meditation_sound_controls(app);

    format_meditation_time(time_text, sizeof(time_text), app->meditation_remaining_seconds);
    max_w = view_width - flint_px(48);
    while(font > flint_px(28) && flint_text_measure(time_text, font) > max_w)
        font--;
    text_w = flint_text_measure(time_text, font);
    flint_text_draw(time_text, center_x - text_w / 2,
                    flint_ui_text_y(time_text, center_y - font, font * 2, font),
                    font, c_text);

    app->meditation_frame_ticks++;
    if(app->meditation_frame_ticks >= 60) {
        app->meditation_frame_ticks = 0;
        if(app->meditation_remaining_seconds > 0)
            app->meditation_remaining_seconds--;
        if(app->meditation_remaining_seconds <= 0)
            meditation_finish(app);
    }
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
        if(app->settings_from_exercise_selector) {
            app->settings_from_exercise_selector = 0;
            app->settings_category = -1;
            app->settings_sub_tab = 0;
            app->settings_scroll = 0;
            app->inbe.screen = InbeScreenStart;
            break;
        }
        if(app->settings_category != -1) {
            app->settings_category = -1;
            app->settings_sub_tab = 0;
            app->settings_scroll = 0;
        } else {
            app->inbe.screen = app->main_tab == APP_MAIN_TAB_HABITS
                                   ? InbeScreenHabits
                                   : InbeScreenStart;
            app->settings_scroll = 0;
        }
        break;

    case InbeScreenHabits:
        app->inbe.screen = InbeScreenStart;
        break;

    case InbeScreenHabitManager:
        app->inbe.screen = InbeScreenHabits;
        break;

    case InbeScreenHabitEdit:
        habit_edit_commit(app);
        break;

    case InbeScreenHabitSessionEdit:
        habit_session_cancel_edit(app);
        app->inbe.screen = InbeScreenHabits;
        break;

    case InbeScreenPracticeConfig:
        if(app->practice_config_theme_tab >= 0) {
            app->practice_config_theme_tab = PRACTICE_CONFIG_TAB_LIST;
            save_settings(app);
        } else {
            save_settings(app);
            app->inbe.screen = InbeScreenStart;
        }
        break;

    case InbeScreenLanguage:
        break;

    case InbeScreenManual:
        manual_tab_close_tutorial(app, 0);
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

    case InbeScreenMeditation:
        meditation_music_stop(app);
        app->meditation_duration_seconds = 0;
        app->meditation_remaining_seconds = 0;
        app->meditation_frame_ticks = 0;
        app->volume_popup_active = 0;
        app->inbe.screen = InbeScreenStart;
        break;

    case InbeScreenStart:
    default:
        break;
    }
}

static void
draw_app_title_bar(const char *title)
{
    int title_font = flint_px(28);
    int top_h = flint_px(58);

    DrawRectangle(0, 0, view_width, top_h, flint_darken(c_bg, 14));
    DrawLine(0, top_h - 1, view_width, top_h - 1, flint_darken(c_bg, 42));
    flint_text_draw(title, flint_px(16),
                    flint_ui_text_y(title, 0, top_h, title_font),
                    title_font, c_text);
}

static void
practice_ensure_enabled_selection(InbeApp *app)
{
    if(app == NULL)
        return;

    if(app->practice_category_tab < 0 || app->practice_category_tab >= PRACTICE_CATEGORY_COUNT)
        app->practice_category_tab = PRACTICE_CATEGORY_MIND;

    if(app->practice_tab_enabled[app->practice_category_tab])
        return;

    for(int i = 0; i < PRACTICE_CATEGORY_COUNT; i++) {
        if(app->practice_tab_enabled[i]) {
            app->practice_category_tab = i;
            return;
        }
    }

    app->practice_tab_enabled[PRACTICE_CATEGORY_MIND] = 1;
    app->practice_category_tab = PRACTICE_CATEGORY_MIND;
}

static int
practice_enabled_count(InbeApp *app)
{
    int count = 0;

    if(app == NULL)
        return 0;

    for(int i = 0; i < PRACTICE_CATEGORY_COUNT; i++) {
        if(app->practice_tab_enabled[i])
            count++;
    }
    return count;
}

static int
practice_active_theme(InbeApp *app)
{
    if(app == NULL)
        return ThemeSky;

    practice_ensure_enabled_selection(app);
    return clampi(app->practice_tab_theme[app->practice_category_tab], 0, THEME_COUNT - 1);
}

static void
practice_sync_global_theme(InbeApp *app)
{
    if(app == NULL)
        return;
    app->theme_id = practice_active_theme(app);
    app_refresh_theme(app);
}

static int
practice_activity_count_for_tab(int tab)
{
    return tab == PRACTICE_CATEGORY_MIND ? 2 : 1;
}

static int
practice_activity_for_tab(int tab, int index)
{
    if(tab == PRACTICE_CATEGORY_MIND) {
        return index == 1 ? EXERCISE_MEDITATION : EXERCISE_WIM_HOF;
    }
    if(tab == PRACTICE_CATEGORY_YOGA)
        return EXERCISE_SUN_SALUTATION;
    if(tab == PRACTICE_CATEGORY_FITNESS)
        return EXERCISE_7_MINUTE_WORKOUT;
    return EXERCISE_WIM_HOF;
}

static const char *
practice_activity_label(int exercise)
{
    switch(exercise) {
    case EXERCISE_MEDITATION:
        return locale_get("exercise_meditation");
    case EXERCISE_SUN_SALUTATION:
        return "Sun Salutation";
    case EXERCISE_7_MINUTE_WORKOUT:
        return "7-Minute Workout";
    case EXERCISE_WIM_HOF:
    default:
        return locale_get("exercise_wim_hof");
    }
}

static int
practice_activity_index_for_tab(int tab, int exercise)
{
    int count = practice_activity_count_for_tab(tab);
    for(int i = 0; i < count; i++) {
        if(practice_activity_for_tab(tab, i) == exercise)
            return i;
    }
    return 0;
}

static void
practice_clamp_activity_to_tab(InbeApp *app)
{
    int tab;
    int index;

    if(app == NULL)
        return;

    practice_ensure_enabled_selection(app);
    tab = app->practice_category_tab;
    index = practice_activity_index_for_tab(tab, app->exercise_type);
    app->exercise_type = practice_activity_for_tab(tab, index);
}

static Color
practice_theme_color(InbeApp *app, int tab_index)
{
    int theme_id = ThemeSky;
    Color color = c_circle;

    if(app != NULL && tab_index >= 0 && tab_index < PRACTICE_CATEGORY_COUNT)
        theme_id = clampi(app->practice_tab_theme[tab_index], 0, THEME_COUNT - 1);

    if(!flint_theme_catalog_color((FlintThemeId)theme_id, app != NULL && app->dark_mode != 0,
                                  "circle", &color)) {
        const char *scope = flint_theme_scope_for((FlintThemeId)theme_id,
                                                  app != NULL && app->dark_mode != 0);
        color = flint_theme_get(scope, "circle");
    }

    return color;
}

static void
habits_sync_topic_theme_colors(InbeApp *app, int sync_topic, int save_now)
{
    Color color;
    int changed = 0;

    if(app == NULL || sync_topic < 0 || sync_topic >= PRACTICE_CATEGORY_COUNT)
        return;

    color = practice_theme_color(app, sync_topic);
    for(int i = 0; i < app->habits.count; i++) {
        InbeHabit *habit = &app->habits.items[i];
        if(habit->sync_mode != INBE_HABIT_SYNC_TOPIC ||
           habit->sync_topic != sync_topic)
            continue;
        if(habit->color.r != color.r || habit->color.g != color.g ||
           habit->color.b != color.b || habit->color.a != 255) {
            habit->color = color;
            habit->color.a = 255;
            changed = 1;
        }
    }

    if(changed && save_now)
        inbe_habits_save(&app->habits);
}

static void
habits_sync_all_topic_theme_colors(InbeApp *app, int save_now)
{
    if(app == NULL)
        return;
    for(int i = 0; i < PRACTICE_CATEGORY_COUNT; i++)
        habits_sync_topic_theme_colors(app, i, 0);
    if(save_now)
        inbe_habits_save(&app->habits);
}

static int
practice_category_bottom_y_for_app(InbeApp *app)
{
    return flint_px(58) +
           (practice_enabled_count(app) > 1 ? flint_px(PRACTICE_CATEGORY_TAB_H) : 0);
}

static void
draw_practice_coming_soon_popout(InbeApp *app)
{
    const char *message = "coming soon...";
    int font = flint_ui_font();
    int pad_x = flint_px(14);
    int pad_y = flint_px(8);
    int tabs_y = flint_px(58);
    int tabs_h = flint_px(PRACTICE_CATEGORY_TAB_H);
    int popout_w = flint_text_measure(message, font) + pad_x * 2;
    int popout_h = font + pad_y * 2;
    int popout_x = (view_width - popout_w) / 2;
    int popout_y = tabs_y + tabs_h + flint_px(8);

    if(app == NULL || app->practice_coming_soon_ticks <= 0)
        return;

    if(popout_x < flint_px(8))
        popout_x = flint_px(8);
    if(popout_x + popout_w > view_width - flint_px(8))
        popout_x = view_width - flint_px(8) - popout_w;

    DrawRectangle(popout_x, popout_y, popout_w, popout_h, c_surface);
    ui_draw_bevel(popout_x, popout_y, popout_w, popout_h,
                  flint_lighten(c_surface, 40), flint_darken(c_surface, 40));
    flint_text_draw(message, popout_x + pad_x,
                    flint_ui_text_y(message, popout_y, popout_h, font),
                    font, c_text);
}

static void
draw_practice_category_tabs(InbeApp *app)
{
    FlintUISubtab tabs[PRACTICE_CATEGORY_COUNT];
    int tab_indexes[PRACTICE_CATEGORY_COUNT];
    int tabs_y = flint_px(58);
    int tabs_h = flint_px(PRACTICE_CATEGORY_TAB_H);
    int enabled_count = 0;
    int selected_visible = 0;
    int clicked;

    if(app == NULL)
        return;

    practice_ensure_enabled_selection(app);
    for(int i = 0; i < PRACTICE_CATEGORY_COUNT; i++) {
        if(!app->practice_tab_enabled[i])
            continue;

        tab_indexes[enabled_count] = i;
        tabs[enabled_count] = (FlintUISubtab){
            .label = g_practice_category_labels[i],
            .disabled = 0,
            .accent = practice_theme_color(app, i)
        };
        if(i == app->practice_category_tab)
            selected_visible = enabled_count;
        enabled_count++;
    }

    if(enabled_count <= 1)
        return;

    clicked = ui_draw_subtab_bar((FlintUISubtabBar){
        .bounds = {0, (float)tabs_y, (float)view_width, (float)tabs_h},
        .tabs = tabs,
        .count = enabled_count,
        .selected_index = selected_visible,
        .font = flint_ui_font()
    });

    if(clicked >= 0 && clicked < enabled_count) {
        app->practice_category_tab = tab_indexes[clicked];
        app->practice_coming_soon_ticks = 0;
        practice_clamp_activity_to_tab(app);
        practice_sync_global_theme(app);
        save_settings(app);
    }
}

static int
draw_theme_circle_button(InbeApp *app, int x, int y, int radius, int theme_id)
{
    Vector2 mouse = GetScreenToWorld2D(GetMousePosition(), app->camera);
    Rectangle bounds = {
        (float)(x - radius - flint_px(5)),
        (float)(y - radius - flint_px(5)),
        (float)(radius * 2 + flint_px(10)),
        (float)(radius * 2 + flint_px(10))
    };
    int hovered = CheckCollisionPointRec(mouse, bounds);
    Color color = c_circle;

    if(!flint_theme_catalog_color((FlintThemeId)theme_id, app->dark_mode != 0, "circle", &color)) {
        const char *scope = flint_theme_scope_for((FlintThemeId)theme_id, app->dark_mode != 0);
        color = flint_theme_get(scope, "circle");
    }

    DrawCircle(x, y, radius, color);
    DrawCircleLines(x, y, radius + flint_px(2), hovered ? c_text : flint_darken(c_bg, 42));

    if(hovered) {
        app->cursor_clickable = 1;
        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && !ui_input_captures_click(mouse))
            return 1;
    }

    return 0;
}

static void
draw_practice_config_button(InbeApp *app)
{
    int icon_size = flint_px(20);
    int icon_padding = flint_px(8);
    int button_w = icon_size + icon_padding * 2;
    int button_x = view_width - button_w - flint_px(10);
    int button_y = (flint_px(58) - button_w) / 2;
    int hover = 0;

    if(app == NULL || app->modal.active)
        return;

    if(ui_draw_icon_btn_padded(button_x, button_y, icon_size, icon_padding,
                               app->icons[UI_ICON_TYPE_STACK], &hover)) {
        app->practice_config_theme_tab = PRACTICE_CONFIG_TAB_LIST;
        app->inbe.screen = InbeScreenPracticeConfig;
    }
}

static void
draw_habits_manager_button(InbeApp *app)
{
    int icon_size = flint_px(20);
    int icon_padding = flint_px(8);
    int button_w = icon_size + icon_padding * 2;
    int button_x = view_width - button_w - flint_px(10);
    int button_y = (flint_px(58) - button_w) / 2;
    int hover = 0;

    if(app == NULL || app->modal.active)
        return;

    if(ui_draw_icon_btn_padded(button_x, button_y, icon_size, icon_padding,
                               app->icons[UI_ICON_TYPE_STACK], &hover)) {
        habit_edit_cancel(app);
        app->inbe.screen = InbeScreenHabitManager;
    }
}

static void
draw_practice_config_page(InbeApp *app)
{
    int top_h = flint_px(58);
    int nav_h = flint_px(TAB_BAR_H);
    int content_x;
    int content_w;
    int max_w = flint_px(CONTENT_MAX_W);
    int y = top_h + flint_px(16);
    int row_h = flint_px(48);
    int enabled_count;
    int coming_soon_y = -1;
    FlintUIHeader header;

    if(app == NULL)
        return;

    if(app->practice_config_theme_tab >= 0) {
        int tab = clampi(app->practice_config_theme_tab, 0, PRACTICE_CATEGORY_COUNT - 1);
        char title[64];

        snprintf(title, sizeof(title), "%s Theme", g_practice_category_labels[tab]);
        header = ui_draw_title_header(top_h, title,
                                      app->icons[UI_ICON_TYPE_RETURN], (Texture2D){0});
        if(header.left_clicked) {
            app->practice_config_theme_tab = PRACTICE_CONFIG_TAB_LIST;
            save_settings(app);
            return;
        }

        flint_centered_column(max_w, flint_page_side_padding(), &content_x, &content_w);
        ui_begin_scissor((int)app->camera.offset.x,
                         (int)(app->camera.offset.y + top_h * app->camera.zoom),
                         (int)(view_width * app->camera.zoom),
                         (int)((view_height - top_h - nav_h) * app->camera.zoom));
        if(ui_draw_theme_picker(content_x, y, content_w, "Theme",
                                app->dark_mode, &app->practice_tab_theme[tab])) {
            app->practice_tab_theme[tab] = clampi(app->practice_tab_theme[tab], 0, THEME_COUNT - 1);
            habits_sync_topic_theme_colors(app, tab, 1);
            if(tab == app->practice_category_tab)
                practice_sync_global_theme(app);
            app->settings_dirty = 1;
            save_settings(app);
        }
        ui_end_scissor();
        return;
    }

    header = ui_draw_title_header(top_h, "Practice Tabs",
                                  app->icons[UI_ICON_TYPE_RETURN], (Texture2D){0});
    if(header.left_clicked) {
        save_settings(app);
        app->inbe.screen = InbeScreenStart;
        return;
    }

    flint_centered_column(max_w, flint_page_side_padding(), &content_x, &content_w);
    ui_begin_scissor((int)app->camera.offset.x,
                     (int)(app->camera.offset.y + top_h * app->camera.zoom),
                     (int)(view_width * app->camera.zoom),
                     (int)((view_height - top_h - nav_h) * app->camera.zoom));

    enabled_count = practice_enabled_count(app);
    for(int i = 0; i < PRACTICE_CATEGORY_COUNT; i++) {
        int enabled = app->practice_tab_enabled[i];
        int unavailable = i != PRACTICE_CATEGORY_MIND;
        int label_x = content_x + flint_px(8);
        int circle_x = content_x + content_w - flint_px(24);
        int circle_y = y + row_h / 2;
        int theme_id = clampi(app->practice_tab_theme[i], 0, THEME_COUNT - 1);
        Rectangle row_bounds = {
            (float)content_x,
            (float)y,
            (float)content_w,
            (float)row_h
        };
        Vector2 mouse = GetScreenToWorld2D(GetMousePosition(), app->camera);
        int row_hover = CheckCollisionPointRec(mouse, row_bounds);
        Color row_bg = unavailable ? flint_darken(c_button, 12) : flint_darken(c_button, 6);
        Color row_text = unavailable ? flint_darken(c_text, 72) : c_text;

        if(unavailable && row_hover) {
            row_bg = flint_darken(c_button, 18);
            app->cursor_disabled = 1;
            coming_soon_y = y + row_h + flint_px(6);
            if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && !ui_input_captures_click(mouse))
                app->practice_coming_soon_ticks = PRACTICE_CATEGORY_TOAST_TICKS;
        }

        DrawRectangleRec(row_bounds, row_bg);
        DrawLine(content_x, y + row_h - 1,
                 content_x + content_w, y + row_h - 1,
                 flint_darken(c_button, 34));

        if(unavailable) {
            int box_size = flint_px(22);
            DrawRectangle(label_x, y + flint_px(12), box_size, box_size,
                          flint_darken(c_button, 24));
            ui_draw_bevel(label_x, y + flint_px(12), box_size, box_size,
                          flint_darken(c_bg, 34), flint_lighten(c_bg, 8));
            flint_text_draw(g_practice_category_labels[i],
                            label_x + box_size + flint_px(10),
                            flint_ui_text_y(g_practice_category_labels[i],
                                            y + flint_px(12), box_size, flint_ui_font()),
                            flint_ui_font(), row_text);
        } else if(ui_draw_checkbox_toggle(label_x, y + flint_px(12),
                                          g_practice_category_labels[i], &enabled)) {
            if(enabled || enabled_count > 1) {
                app->practice_tab_enabled[i] = enabled;
                practice_ensure_enabled_selection(app);
                app->settings_dirty = 1;
                enabled_count = practice_enabled_count(app);
            }
        }

        if(unavailable) {
            Color color = practice_theme_color(app, i);
            color.a = 120;
            DrawCircle(circle_x, circle_y, flint_px(13), color);
            DrawCircleLines(circle_x, circle_y, flint_px(15), flint_darken(c_bg, 45));
        } else if(draw_theme_circle_button(app, circle_x, circle_y, flint_px(13), theme_id)) {
            app->practice_config_theme_tab = i;
        }

        y += row_h;
    }

    if(coming_soon_y >= 0) {
        const char *message = "coming soon...";
        int font = flint_ui_font();
        int pad_x = flint_px(12);
        int pad_y = flint_px(7);
        int popout_w = flint_text_measure(message, font) + pad_x * 2;
        int popout_h = font + pad_y * 2;
        int popout_x = content_x + content_w - popout_w;

        DrawRectangle(popout_x, coming_soon_y, popout_w, popout_h, c_surface);
        ui_draw_bevel(popout_x, coming_soon_y, popout_w, popout_h,
                      flint_lighten(c_surface, 40), flint_darken(c_surface, 40));
        flint_text_draw(message, popout_x + pad_x,
                        flint_ui_text_y(message, coming_soon_y, popout_h, font),
                        font, c_text);
    }

    ui_end_scissor();
    if(app->settings_dirty)
        save_settings(app);
}

static void
habit_edit_clamp_cursor(InbeApp *app)
{
    int len;

    if(app == NULL)
        return;

    len = (int)strlen(app->habit_edit_text);
    if(app->habit_edit_cursor < 0)
        app->habit_edit_cursor = 0;
    if(app->habit_edit_cursor > len)
        app->habit_edit_cursor = len;
}

static void
habit_edit_begin(InbeApp *app, int index)
{
    if(app == NULL || index < 0 || index >= app->habits.count)
        return;

    snprintf(app->habit_edit_text, sizeof(app->habit_edit_text), "%s",
             app->habits.items[index].name);
    app->habit_edit_active = 1;
    app->habit_edit_is_new = 0;
    app->habit_edit_index = index;
    app->habit_edit_cursor = (int)strlen(app->habit_edit_text);
    app->habit_edit_color = app->habits.items[index].color;
    app->habit_edit_sync_mode = app->habits.items[index].sync_mode;
    app->habit_edit_sync_topic = app->habits.items[index].sync_topic;
    app->habit_edit_sync_activity = app->habits.items[index].sync_activity;
    app->inbe.screen = InbeScreenHabitEdit;
}

static void
habit_edit_begin_new(InbeApp *app)
{
    if(app == NULL)
        return;

    snprintf(app->habit_edit_text, sizeof(app->habit_edit_text), "%s", "New Habit");
    app->habit_edit_active = 1;
    app->habit_edit_is_new = 1;
    app->habit_edit_index = -1;
    app->habit_edit_cursor = (int)strlen(app->habit_edit_text);
    app->habit_edit_color = (Color){99, 196, 165, 255};
    app->habit_edit_sync_mode = INBE_HABIT_SYNC_NONE;
    app->habit_edit_sync_topic = INBE_HABIT_TOPIC_MIND;
    app->habit_edit_sync_activity = EXERCISE_WIM_HOF;
    app->inbe.screen = InbeScreenHabitEdit;
}

static void
habit_edit_cancel(InbeApp *app)
{
    if(app == NULL)
        return;

    app->habit_edit_active = 0;
    app->habit_edit_is_new = 0;
    app->habit_edit_index = -1;
    app->habit_edit_cursor = 0;
    app->habit_edit_text[0] = '\0';
}

static const char *
habit_edit_trimmed_text(InbeApp *app)
{
    char *start;
    char *end;

    if(app == NULL)
        return "";

    start = app->habit_edit_text;
    while(*start == ' ' || *start == '\t')
        start++;
    end = start + strlen(start);
    while(end > start && (end[-1] == ' ' || end[-1] == '\t'))
        end--;
    *end = '\0';
    return start;
}

static void
habit_edit_commit(InbeApp *app)
{
    const char *text;
    int index;

    if(app == NULL || !app->habit_edit_active)
        return;

    index = app->habit_edit_index;
    if(!app->habit_edit_is_new && (index < 0 || index >= app->habits.count)) {
        habit_edit_cancel(app);
        return;
    }

    text = habit_edit_trimmed_text(app);
    if(text[0] != '\0') {
        if(app->habit_edit_sync_mode == INBE_HABIT_SYNC_TOPIC) {
            app->habit_edit_sync_topic = clampi(app->habit_edit_sync_topic,
                                                0, INBE_HABIT_TOPIC_COUNT - 1);
            app->habit_edit_color = practice_theme_color(app, app->habit_edit_sync_topic);
        }
        if(app->habit_edit_is_new) {
            inbe_habits_add_custom(&app->habits, text, app->habit_edit_color,
                                   app->habit_edit_sync_mode,
                                   app->habit_edit_sync_topic,
                                   app->habit_edit_sync_activity);
        } else {
            snprintf(app->habits.items[index].name,
                     sizeof(app->habits.items[index].name), "%s", text);
            app->habits.items[index].color = app->habit_edit_color;
            app->habits.items[index].color.a = 255;
            app->habits.items[index].sync_mode = app->habit_edit_sync_mode;
            app->habits.items[index].sync_topic = app->habit_edit_sync_topic;
            app->habits.items[index].sync_activity = app->habit_edit_sync_activity;
            app->habits.selected = index;
            inbe_habits_save(&app->habits);
        }
    }
    habit_edit_cancel(app);
    app->inbe.screen = InbeScreenHabitManager;
}

static void
habit_edit_delete_before_cursor(InbeApp *app)
{
    int len;

    if(app == NULL)
        return;

    habit_edit_clamp_cursor(app);
    if(app->habit_edit_cursor <= 0)
        return;

    len = (int)strlen(app->habit_edit_text);
    memmove(app->habit_edit_text + app->habit_edit_cursor - 1,
            app->habit_edit_text + app->habit_edit_cursor,
            (size_t)(len - app->habit_edit_cursor + 1));
    app->habit_edit_cursor--;
}

static void
habit_edit_delete_at_cursor(InbeApp *app)
{
    int len;

    if(app == NULL)
        return;

    habit_edit_clamp_cursor(app);
    len = (int)strlen(app->habit_edit_text);
    if(app->habit_edit_cursor >= len)
        return;

    memmove(app->habit_edit_text + app->habit_edit_cursor,
            app->habit_edit_text + app->habit_edit_cursor + 1,
            (size_t)(len - app->habit_edit_cursor));
}

static void
habit_edit_insert_char(InbeApp *app, char ch)
{
    int len;

    if(app == NULL)
        return;

    habit_edit_clamp_cursor(app);
    len = (int)strlen(app->habit_edit_text);
    if(len >= (int)sizeof(app->habit_edit_text) - 1)
        return;

    memmove(app->habit_edit_text + app->habit_edit_cursor + 1,
            app->habit_edit_text + app->habit_edit_cursor,
            (size_t)(len - app->habit_edit_cursor + 1));
    app->habit_edit_text[app->habit_edit_cursor] = ch;
    app->habit_edit_cursor++;
}

static int
habit_edit_cursor_from_x(const char *text, int font, int text_x, int mouse_x)
{
    char prefix[INBE_HABIT_NAME_SIZE];
    int len;

    if(text == NULL)
        return 0;

    len = (int)strlen(text);
    if(mouse_x <= text_x)
        return 0;

    for(int i = 1; i <= len; i++) {
        snprintf(prefix, sizeof(prefix), "%.*s", i, text);
        if(text_x + flint_text_measure(prefix, font) >= mouse_x)
            return i;
    }
    return len;
}

static void
habit_edit_update_input(InbeApp *app, int field_x, int field_y, int field_w,
                        int field_h, int text_x, int font)
{
    int ch;

    if(app == NULL || !app->habit_edit_active)
        return;

    habit_edit_clamp_cursor(app);

    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
        int mx = (int)mouse_world.x;
        int my = (int)mouse_world.y;

        if(mx >= field_x && mx <= field_x + field_w &&
           my >= field_y && my <= field_y + field_h) {
            app->habit_edit_cursor = habit_edit_cursor_from_x(app->habit_edit_text,
                                                              font, text_x, mx);
        }
    }

    if(IsKeyPressed(KEY_ESCAPE)) {
        habit_edit_cancel(app);
        return;
    }
    if(IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        habit_edit_commit(app);
        return;
    }
    if(IsKeyPressed(KEY_LEFT))
        app->habit_edit_cursor--;
    if(IsKeyPressed(KEY_RIGHT))
        app->habit_edit_cursor++;
    if(IsKeyPressed(KEY_HOME))
        app->habit_edit_cursor = 0;
    if(IsKeyPressed(KEY_END))
        app->habit_edit_cursor = (int)strlen(app->habit_edit_text);
    habit_edit_clamp_cursor(app);

    if(IsKeyPressed(KEY_BACKSPACE))
        habit_edit_delete_before_cursor(app);
    if(IsKeyPressed(KEY_DELETE))
        habit_edit_delete_at_cursor(app);

    ch = GetCharPressed();
    while(ch > 0) {
        if(ch >= 32 && ch <= 126)
            habit_edit_insert_char(app, (char)ch);
        ch = GetCharPressed();
    }

    habit_edit_clamp_cursor(app);
}

static void
draw_habit_edit_field(InbeApp *app, int x, int y, int w, int h, int font)
{
    FlintUITextInputStyle style = {
        .background = flint_darken(c_bg, 4),
        .border = c_button,
        .focus_border = c_button_hover,
        .text = c_text,
        .cursor = c_text,
        .radius = 0.08f,
        .padding_x = flint_px(10)
    };

    flint_ui_draw_text_input((Rectangle){(float)x, (float)y, (float)w, (float)h},
                             app != NULL ? app->habit_edit_text : "",
                             app != NULL ? app->habit_edit_cursor : 0,
                             1, ((app != NULL ? app->inbe.frame : 0) / 24) % 2,
                             font, style);
}

enum {
    HABIT_SESSION_EDIT_NONE = 0,
    HABIT_SESSION_EDIT_TIME = 1,
    HABIT_SESSION_EDIT_ROUND = 2,
    HABIT_LINKED_ENTRY_MAX = 128,
    HABIT_LINKED_PATH_SIZE = 80
};

typedef struct HabitLinkedEntry {
    char path[HABIT_LINKED_PATH_SIZE];
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    int rounds[MaxRounds];
    int round_count;
    int total_seconds;
    int best_seconds;
} HabitLinkedEntry;

typedef struct HabitLinkedContext {
    HabitLinkedEntry entries[HABIT_LINKED_ENTRY_MAX];
    int count;
    int day_filter;
    int sync_mode;
    int sync_topic;
    int sync_activity;
    int total_seconds;
    int best_seconds;
} HabitLinkedContext;

static int
habit_is_linked(const InbeHabit *habit)
{
    return habit != NULL && habit->sync_mode != INBE_HABIT_SYNC_NONE;
}

static int
habit_date_index(int year, int month, int day)
{
    return year * 10000 + month * 100 + day;
}

static void
habit_format_date(int day_index, char *out, size_t out_size)
{
    if(out == NULL || out_size == 0)
        return;
    snprintf(out, out_size, "%04d-%02d-%02d",
             day_index / 10000, (day_index / 100) % 100, day_index % 100);
}

static const char *
habit_month_label(int month)
{
    static const char *months[] = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };

    if(month < 1 || month > 12)
        return "";
    return months[month - 1];
}

static void
habit_linked_session_callback(const char *path, int year, int month, int day,
                              int hour, int minute, int second,
                              int topic, int activity,
                              const int *round_times, int round_count, void *user)
{
    HabitLinkedContext *ctx = user;
    HabitLinkedEntry *entry;
    int day_index = habit_date_index(year, month, day);

    (void)path;
    if(ctx == NULL || round_times == NULL || round_count <= 0)
        return;
    if(ctx->day_filter > 0 && ctx->day_filter != day_index)
        return;
    if(ctx->sync_mode == INBE_HABIT_SYNC_TOPIC && topic != ctx->sync_topic)
        return;
    if(ctx->sync_mode == INBE_HABIT_SYNC_ACTIVITY && activity != ctx->sync_activity)
        return;
    if(ctx->count >= HABIT_LINKED_ENTRY_MAX)
        return;

    entry = &ctx->entries[ctx->count++];
    memset(entry, 0, sizeof(*entry));
    snprintf(entry->path, sizeof(entry->path), "%s", path != NULL ? path : "");
    entry->year = year;
    entry->month = month;
    entry->day = day;
    entry->hour = hour;
    entry->minute = minute;
    entry->second = second;
    entry->round_count = round_count > MaxRounds ? MaxRounds : round_count;
    for(int i = 0; i < entry->round_count; i++) {
        entry->rounds[i] = round_times[i];
        entry->total_seconds += round_times[i];
        if(round_times[i] > entry->best_seconds)
            entry->best_seconds = round_times[i];
    }
    ctx->total_seconds += entry->total_seconds;
    if(entry->best_seconds > ctx->best_seconds)
        ctx->best_seconds = entry->best_seconds;
}

static void
habit_collect_linked_entries(const InbeHabit *habit, int day_filter, HabitLinkedContext *ctx)
{
    if(ctx == NULL)
        return;
    memset(ctx, 0, sizeof(*ctx));
    ctx->day_filter = day_filter;
    ctx->sync_mode = habit != NULL ? habit->sync_mode : INBE_HABIT_SYNC_NONE;
    ctx->sync_topic = habit != NULL ? habit->sync_topic : INBE_HABIT_TOPIC_MIND;
    ctx->sync_activity = habit != NULL ? habit->sync_activity : EXERCISE_WIM_HOF;
    data_list_session_records(habit_linked_session_callback, ctx);
}

static int
habit_linked_has_day(const HabitLinkedContext *ctx, int day_index)
{
    if(ctx == NULL)
        return 0;
    for(int i = 0; i < ctx->count; i++) {
        if(habit_date_index(ctx->entries[i].year, ctx->entries[i].month, ctx->entries[i].day) == day_index)
            return 1;
    }
    return 0;
}

static void
habit_open_linked_details(InbeApp *app, int habit_index, int day_index)
{
    if(app == NULL)
        return;
    app->habit_detail_index = habit_index;
    app->habit_detail_day = day_index;
    app->habit_detail_session_index = -1;
    app->habit_detail_session_path[0] = '\0';
    app->habit_session_edit_active = 0;
    app->habit_session_edit_kind = HABIT_SESSION_EDIT_NONE;
    app->habit_session_edit_round = -1;
    app->habit_session_edit_path[0] = '\0';
    app->habit_session_edit_text[0] = '\0';
    app->modal.active = 1;
    app->modal.type = UIModalHabitLinkedDetails;
}

static void
habit_open_session_edit_page(InbeApp *app)
{
    if(app == NULL)
        return;
    if(app->habit_detail_index < 0 || app->habit_detail_index >= app->habits.count)
        return;
    app->modal.active = 0;
    app->modal.type = UIModalNone;
    app->habit_session_edit_scroll = 0;
    app->habit_session_edit_active = 0;
    app->habit_session_edit_kind = HABIT_SESSION_EDIT_NONE;
    app->habit_session_edit_round = -1;
    app->habit_session_edit_path[0] = '\0';
    app->habit_session_edit_text[0] = '\0';
    app->inbe.screen = InbeScreenHabitSessionEdit;
}

static void
habit_session_cancel_edit(InbeApp *app)
{
    if(app == NULL)
        return;
    app->habit_session_edit_active = 0;
    app->habit_session_edit_kind = HABIT_SESSION_EDIT_NONE;
    app->habit_session_edit_round = -1;
    app->habit_session_edit_cursor = 0;
    app->habit_session_edit_path[0] = '\0';
    app->habit_session_edit_text[0] = '\0';
}

static int
habit_session_edit_matches(InbeApp *app, const HabitLinkedEntry *entry, int kind, int round)
{
    return app != NULL && entry != NULL &&
           app->habit_session_edit_active &&
           app->habit_session_edit_kind == kind &&
           app->habit_session_edit_round == round &&
           strcmp(app->habit_session_edit_path, entry->path) == 0;
}

static void
habit_session_begin_edit_time(InbeApp *app, const HabitLinkedEntry *entry)
{
    if(app == NULL || entry == NULL)
        return;
    app->habit_session_edit_active = 1;
    app->habit_session_edit_kind = HABIT_SESSION_EDIT_TIME;
    app->habit_session_edit_round = -1;
    snprintf(app->habit_session_edit_path, sizeof(app->habit_session_edit_path), "%s", entry->path);
    snprintf(app->habit_session_edit_text, sizeof(app->habit_session_edit_text), "%02d:%02d",
             entry->hour, entry->minute);
    app->habit_session_edit_cursor = (int)strlen(app->habit_session_edit_text);
}

static void
habit_session_begin_edit_round(InbeApp *app, const HabitLinkedEntry *entry, int round)
{
    if(app == NULL || entry == NULL || round < 0 || round >= entry->round_count)
        return;
    app->habit_session_edit_active = 1;
    app->habit_session_edit_kind = HABIT_SESSION_EDIT_ROUND;
    app->habit_session_edit_round = round;
    snprintf(app->habit_session_edit_path, sizeof(app->habit_session_edit_path), "%s", entry->path);
    snprintf(app->habit_session_edit_text, sizeof(app->habit_session_edit_text), "%d",
             entry->rounds[round]);
    app->habit_session_edit_cursor = (int)strlen(app->habit_session_edit_text);
}

static int
habit_session_parse_time(const char *text, int *hour, int *minute)
{
    int h;
    int m;
    char tail;

    if(text == NULL || sscanf(text, "%d:%d%c", &h, &m, &tail) != 2)
        return 0;
    if(h < 0 || h > 23 || m < 0 || m > 59)
        return 0;
    if(hour != NULL)
        *hour = h;
    if(minute != NULL)
        *minute = m;
    return 1;
}

static int
habit_session_parse_seconds(const char *text, int *seconds)
{
    int value;
    char tail;

    if(text == NULL || sscanf(text, "%d%c", &value, &tail) != 1)
        return 0;
    if(value <= 0 || value > 999)
        return 0;
    if(seconds != NULL)
        *seconds = value;
    return 1;
}

static int
habit_session_commit_edit(InbeApp *app, const HabitLinkedEntry *entry)
{
    if(app == NULL || entry == NULL || !app->habit_session_edit_active)
        return 0;

    if(app->habit_session_edit_kind == HABIT_SESSION_EDIT_TIME) {
        int hour;
        int minute;
        char new_path[FS_PATH_MAX];
        char dir[FS_PATH_MAX];
        char *slash;

        if(!habit_session_parse_time(app->habit_session_edit_text, &hour, &minute))
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
        habit_session_cancel_edit(app);
        return 1;
    }

    if(app->habit_session_edit_kind == HABIT_SESSION_EDIT_ROUND) {
        int seconds;
        int rounds[MaxRounds];

        if(app->habit_session_edit_round < 0 ||
           app->habit_session_edit_round >= entry->round_count)
            return 0;
        if(!habit_session_parse_seconds(app->habit_session_edit_text, &seconds))
            return 0;
        for(int i = 0; i < entry->round_count; i++)
            rounds[i] = entry->rounds[i];
        rounds[app->habit_session_edit_round] = seconds;
        if(!data_replace_session(entry->path, rounds, entry->round_count))
            return 0;
        habit_session_cancel_edit(app);
        return 1;
    }

    return 0;
}

static void
habit_session_clamp_cursor(InbeApp *app)
{
    int len;

    if(app == NULL)
        return;
    len = (int)strlen(app->habit_session_edit_text);
    if(app->habit_session_edit_cursor < 0)
        app->habit_session_edit_cursor = 0;
    if(app->habit_session_edit_cursor > len)
        app->habit_session_edit_cursor = len;
}

static void
habit_session_delete_before_cursor(InbeApp *app)
{
    size_t len;
    int cursor;

    if(app == NULL)
        return;
    habit_session_clamp_cursor(app);
    len = strlen(app->habit_session_edit_text);
    cursor = app->habit_session_edit_cursor;
    if(cursor <= 0 || len == 0)
        return;
    memmove(app->habit_session_edit_text + cursor - 1,
            app->habit_session_edit_text + cursor,
            len - (size_t)cursor + 1);
    app->habit_session_edit_cursor--;
}

static void
habit_session_insert_char(InbeApp *app, char c)
{
    size_t len;
    int max_len;
    int cursor;

    if(app == NULL)
        return;
    habit_session_clamp_cursor(app);
    len = strlen(app->habit_session_edit_text);
    max_len = app->habit_session_edit_kind == HABIT_SESSION_EDIT_TIME ? 5 : 3;
    cursor = app->habit_session_edit_cursor;

    if(app->habit_session_edit_kind == HABIT_SESSION_EDIT_TIME &&
       c >= '0' && c <= '9' &&
       cursor < (int)len &&
       app->habit_session_edit_text[cursor] == ':') {
        cursor++;
        app->habit_session_edit_cursor = cursor;
    }

    if(len < (size_t)max_len) {
        memmove(app->habit_session_edit_text + cursor + 1,
                app->habit_session_edit_text + cursor,
                len - (size_t)cursor + 1);
        app->habit_session_edit_text[cursor] = c;
        app->habit_session_edit_cursor = cursor + 1;
        return;
    }

    if(cursor < (int)len) {
        if(app->habit_session_edit_kind == HABIT_SESSION_EDIT_TIME &&
           app->habit_session_edit_text[cursor] == ':' &&
           c != ':')
            return;
        app->habit_session_edit_text[cursor] = c;
        app->habit_session_edit_cursor = cursor + 1;
    }
}

static int
habit_session_edit_cursor_from_x(const char *text, int font, int text_x, int target_x)
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
habit_session_update_edit_input(InbeApp *app, const HabitLinkedEntry *entry,
                                int field_x, int field_y, int field_w, int field_h,
                                int text_x, int font)
{
    int ch;

    if(app == NULL || !app->habit_session_edit_active)
        return;
    habit_session_clamp_cursor(app);

    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
        int mx = (int)mouse_world.x;
        int my = (int)mouse_world.y;
        if(mx >= field_x && mx <= field_x + field_w &&
           my >= field_y && my <= field_y + field_h) {
            app->habit_session_edit_cursor = habit_session_edit_cursor_from_x(
                app->habit_session_edit_text, font, text_x, mx);
        }
    }

    if(IsKeyPressed(KEY_ESCAPE)) {
        habit_session_cancel_edit(app);
        return;
    }
    if(IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        habit_session_commit_edit(app, entry);
        return;
    }
    if(IsKeyPressed(KEY_LEFT))
        app->habit_session_edit_cursor--;
    if(IsKeyPressed(KEY_RIGHT))
        app->habit_session_edit_cursor++;
    if(IsKeyPressed(KEY_HOME))
        app->habit_session_edit_cursor = 0;
    if(IsKeyPressed(KEY_END))
        app->habit_session_edit_cursor = (int)strlen(app->habit_session_edit_text);
    habit_session_clamp_cursor(app);

    if(IsKeyPressed(KEY_BACKSPACE))
        habit_session_delete_before_cursor(app);

    ch = GetCharPressed();
    while(ch > 0) {
        int allowed = 0;
        if(ch >= '0' && ch <= '9')
            allowed = 1;
        if(app->habit_session_edit_kind == HABIT_SESSION_EDIT_TIME && ch == ':')
            allowed = 1;
        if(allowed)
            habit_session_insert_char(app, (char)ch);
        ch = GetCharPressed();
    }
}

static void
habit_session_draw_edit_field(InbeApp *app, const HabitLinkedEntry *entry,
                              int x, int y, int w, int h, int font)
{
    FlintUITextInputStyle style = {
        .background = flint_darken(c_bg, 10),
        .border = c_button,
        .focus_border = c_button_hover,
        .text = c_text,
        .cursor = c_text,
        .radius = 0.08f,
        .padding_x = flint_px(8)
    };

    habit_session_update_edit_input(app, entry, x, y + flint_px(3), w, h - flint_px(6),
                                    x + flint_px(8), font);
    if(app == NULL || !app->habit_session_edit_active)
        return;
    flint_ui_draw_text_input((Rectangle){(float)x, (float)(y + flint_px(3)),
                                         (float)w, (float)(h - flint_px(6))},
                             app->habit_session_edit_text,
                             app->habit_session_edit_cursor,
                             1, (app->inbe.frame / 24) % 2,
                             font, style);
}

static int
habit_session_keyboard_key(int x, int y, int w, int h, const char *label)
{
    int hover = 0;
    return ui_draw_generic_button(x, y, w, h, label, UI_BUTTON_STYLE_SECONDARY, 0, &hover);
}

static int
habit_session_keyboard_height(InbeApp *app)
{
    int key_h = flint_px(48);
    int gap = flint_px(6);
    int pad = flint_px(10);

#if !(defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID))
    if(app == NULL || !app->on_screen_keyboard_enabled)
        return 0;
#endif
    if(app == NULL || !app->habit_session_edit_active)
        return 0;
    return pad * 2 + key_h * 4 + gap * 3;
}

static int
habit_session_draw_keyboard(InbeApp *app, const HabitLinkedEntry *entry)
{
    const char *labels[12] = {
        "1", "2", "3",
        "4", "5", "6",
        "7", "8", "9",
        "DEL", "0", "OK"
    };
    int key_h = flint_px(48);
    int gap = flint_px(6);
    int pad = flint_px(10);
    int keyboard_h = habit_session_keyboard_height(app);
    int x = flint_page_side_padding();
    int y = view_height - keyboard_h;
    int w = view_width - x * 2;
    int key_w = (w - gap * 2) / 3;

#if !(defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID))
    if(app == NULL || !app->on_screen_keyboard_enabled)
        return 0;
#endif
    if(app == NULL || !app->habit_session_edit_active || keyboard_h <= 0)
        return 0;

    DrawRectangle(0, y, view_width, keyboard_h, flint_darken(c_bg, 10));
    DrawLine(0, y, view_width, y, flint_darken(c_bg, 42));

    for(int i = 0; i < 12; i++) {
        int col = i % 3;
        int row = i / 3;
        int key_x = x + col * (key_w + gap);
        int key_y = y + pad + row * (key_h + gap);
        if(habit_session_keyboard_key(key_x, key_y, key_w, key_h, labels[i])) {
            if(i == 9) {
                habit_session_delete_before_cursor(app);
            } else if(i == 11) {
                if(habit_session_commit_edit(app, entry))
                    return 1;
            } else {
                habit_session_insert_char(app, labels[i][0]);
            }
        }
    }

    return 0;
}

static int
habit_session_delete_round(const HabitLinkedEntry *entry, int round)
{
    int rounds[MaxRounds];
    int count = 0;

    if(entry == NULL || round < 0 || round >= entry->round_count)
        return 0;
    if(entry->round_count <= 1)
        return data_delete_session(entry->path);
    for(int i = 0; i < entry->round_count; i++) {
        if(i == round)
            continue;
        rounds[count++] = entry->rounds[i];
    }
    return data_replace_session(entry->path, rounds, count);
}

static int
compare_habit_linked_entries(const void *a, const void *b)
{
    const HabitLinkedEntry *ea = a;
    const HabitLinkedEntry *eb = b;
    if(ea->year != eb->year) return eb->year - ea->year;
    if(ea->month != eb->month) return eb->month - ea->month;
    if(ea->day != eb->day) return eb->day - ea->day;
    if(ea->hour != eb->hour) return eb->hour - ea->hour;
    if(ea->minute != eb->minute) return eb->minute - ea->minute;
    return eb->second - ea->second;
}

static void
habit_filter_ctx_to_session(HabitLinkedContext *ctx, const char *path)
{
    HabitLinkedEntry match;

    if(ctx == NULL || path == NULL || path[0] == '\0')
        return;

    for(int i = 0; i < ctx->count; i++) {
        if(strcmp(ctx->entries[i].path, path) == 0) {
            match = ctx->entries[i];
            memset(ctx->entries, 0, sizeof(ctx->entries));
            ctx->entries[0] = match;
            ctx->count = 1;
            ctx->total_seconds = match.total_seconds;
            ctx->best_seconds = match.best_seconds;
            return;
        }
    }

    ctx->count = 0;
    ctx->total_seconds = 0;
    ctx->best_seconds = 0;
}

static int
draw_habit_cascade_row(InbeApp *app, int x, int y, int w, int h,
                       const char *text, int selected, int indent)
{
    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
    int hover = CheckCollisionPointRec(mouse_world, (Rectangle){(float)x, (float)y, (float)w, (float)h});

    if(hover) {
        DrawRectangle(x, y, w, h, selected ? c_button_hover : flint_darken(c_button_hover, 6));
        ui_draw_bevel(x, y, w, h, flint_darken(c_button_hover, 40), flint_lighten(c_button_hover, 40));
        app->cursor_clickable = 1;
        if(IsMouseButtonDown(MOUSE_BUTTON_LEFT))
            ui_draw_bevel(x, y, w, h, flint_lighten(c_button_hover, 40), flint_darken(c_button_hover, 40));
    } else {
        DrawRectangle(x, y, w, h, selected ? c_button : flint_darken(c_bg, 6));
        ui_draw_bevel(x, y, w, h, flint_lighten(c_button, 28), flint_darken(c_button, 20));
    }

    flint_text_draw(text, x + flint_px(indent),
                    flint_ui_text_y(text, y, h, flint_px(16)),
                    flint_px(16), c_text);
    return hover && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

static int
draw_habit_cascade_action(int right_x, int y, int row_h,
                          Texture2D icon)
{
    int icon_size = flint_px(16);
    int icon_padding = flint_px(4);
    int btn_w = icon_size + icon_padding * 2;
    int hover = 0;

    return ui_draw_icon_btn_padded(right_x - btn_w - flint_px(4),
                                   y + (row_h - btn_w) / 2,
                                   icon_size, icon_padding, icon, &hover);
}

static void
habit_open_session_edit_page_for_session(InbeApp *app, int habit_index,
                                         int day_index, const char *path)
{
    if(app == NULL || path == NULL || path[0] == '\0')
        return;
    app->habit_detail_index = habit_index;
    app->habit_detail_day = day_index;
    app->habit_detail_session_index = 0;
    snprintf(app->habit_detail_session_path,
             sizeof(app->habit_detail_session_path), "%s", path);
    habit_open_session_edit_page(app);
}

static void
draw_habit_view_button(InbeApp *app)
{
    int stack_icon_size = flint_px(20);
    int stack_padding = flint_px(8);
    int stack_w = stack_icon_size + stack_padding * 2;
    int button_w = flint_px(68);
    int button_h = flint_px(34);
    int button_x = view_width - stack_w - flint_px(10) - button_w - flint_px(8);
    int button_y = (flint_px(58) - button_h) / 2;
    int hover = 0;

    if(app == NULL || app->modal.active)
        return;
    if(button_x < flint_px(92))
        return;

    if(ui_draw_generic_button(button_x, button_y, button_w, button_h, "View",
                              UI_BUTTON_STYLE_SECONDARY, 0, &hover)) {
        app->habits_view_mode = app->habits_view_mode == HABITS_VIEW_CALENDAR
                                    ? HABITS_VIEW_LINKED
                                    : HABITS_VIEW_CALENDAR;
        app->habits_list_scroll = 0;
        app->habits_list_expanded_year = 0;
        app->habits_list_expanded_month = 0;
        app->habits_list_expanded_day = 0;
        app->habits_list_expanded_session = -1;
        save_settings(app);
    }
}

static void
draw_habit_linked_details_modal(InbeApp *app)
{
    HabitLinkedContext ctx;
    InbeHabit *habit;
    char date_text[32];
    int modal_w = flint_px(350);
    int modal_h = flint_px(330);
    int y;
    int row_h = flint_px(28);
    FlintUIPanelFrame frame;

    if(app == NULL || !app->modal.active || app->modal.type != UIModalHabitLinkedDetails)
        return;
    if(app->habit_detail_index < 0 || app->habit_detail_index >= app->habits.count) {
        app->modal.active = 0;
        app->modal.type = UIModalNone;
        return;
    }

    habit = &app->habits.items[app->habit_detail_index];
    habit_collect_linked_entries(habit, app->habit_detail_day, &ctx);
    if(ctx.count > 1)
        qsort(ctx.entries, (size_t)ctx.count, sizeof(ctx.entries[0]), compare_habit_linked_entries);
    habit_filter_ctx_to_session(&ctx, app->habit_detail_session_path);
    habit_format_date(app->habit_detail_day, date_text, sizeof(date_text));

    frame = ui_draw_modal_frame(modal_w, modal_h, date_text,
                                (Texture2D){0}, app->icons[UI_ICON_TYPE_X]);

    if(frame.right_clicked) {
        app->modal.active = 0;
        app->modal.type = UIModalNone;
        return;
    }

    y = frame.content_y;

    if(ctx.count <= 0) {
        flint_text_draw("No sessions", frame.content_x, y,
                        flint_ui_font(), c_text);
        return;
    }

    {
        char summary[128];
        snprintf(summary, sizeof(summary), "%d session%s",
                 ctx.count, ctx.count == 1 ? "" : "s");
        flint_text_draw(summary, frame.content_x, y,
                        flint_ui_font(), c_text);
        y += flint_px(34);
    }

    for(int i = 0; i < ctx.count && y + row_h < frame.y + frame.h - flint_px(14); i++) {
        char line[128];
        int icon_size = flint_px(18);
        int icon_padding = flint_px(6);
        int icon_w = icon_size + icon_padding * 2;
        int edit_x = frame.content_x + frame.content_w - icon_w;
        int line_w = edit_x - frame.content_x - flint_px(8);
        int hover = 0;

        if(line_w < flint_px(80))
            line_w = flint_px(80);
        snprintf(line, sizeof(line), "%02d:%02d  %d rounds",
                 ctx.entries[i].hour, ctx.entries[i].minute,
                 ctx.entries[i].round_count);
        flint_text_draw_fitted_in_rect(line,
                                       (Rectangle){frame.content_x, y, line_w, row_h},
                                       flint_ui_font_small(), flint_px(10), c_text);
        if(ui_draw_icon_btn_padded(edit_x, y - flint_px(4), icon_size, icon_padding,
                                   app->icons[UI_ICON_TYPE_PENCIL], &hover)) {
            habit_open_session_edit_page_for_session(app, app->habit_detail_index,
                                                     app->habit_detail_day,
                                                     ctx.entries[i].path);
            return;
        }
        y += row_h;

        for(int r = 0; r < ctx.entries[i].round_count &&
            y + row_h < frame.y + frame.h - flint_px(14); r++) {
            char round_line[64];
            snprintf(round_line, sizeof(round_line), "Round %d  %ds",
                     r + 1, ctx.entries[i].rounds[r]);
            flint_text_draw(round_line, frame.content_x + flint_px(16), y,
                            flint_ui_font_small(), flint_darken(c_text, 24));
            y += flint_px(24);
        }
        y += flint_px(4);
    }
}

static int
draw_habit_session_edit_content(InbeApp *app, HabitLinkedContext *ctx,
                                int content_x, int content_w, int y, int draw)
{
    int row_h = flint_px(36);
    int gap = flint_px(8);
    int icon_size = flint_px(18);
    int icon_padding = flint_px(7);
    int icon_w = icon_size + icon_padding * 2;
    int font = flint_ui_font_small();

    if(app == NULL || ctx == NULL)
        return y;

    if(ctx->count <= 0) {
        if(draw)
            flint_text_draw("No sessions", content_x, y, flint_ui_font(), c_text);
        return y + row_h;
    }

    for(int i = 0; i < ctx->count; i++) {
        HabitLinkedEntry *entry = &ctx->entries[i];
        char line[96];
        int right_x = content_x + content_w - icon_w;
        int edit_x = right_x - icon_w - flint_px(6);
        int field_w = edit_x - content_x - flint_px(8);
        int hover = 0;
        int edit_time;

        if(field_w < flint_px(120))
            field_w = flint_px(120);
        edit_time = habit_session_edit_matches(app, entry, HABIT_SESSION_EDIT_TIME, -1);
        snprintf(line, sizeof(line), "%02d:%02d  %d rounds",
                 entry->hour, entry->minute, entry->round_count);

        if(draw) {
            DrawRectangle(content_x, y, content_w, row_h,
                          app->habit_detail_session_index == i
                              ? flint_lighten(c_button, 8)
                              : flint_darken(c_bg, 5));
            DrawRectangle(content_x, y, flint_px(4), row_h, c_circle);
            if(edit_time) {
                habit_session_draw_edit_field(app, entry, content_x + flint_px(10), y,
                                              field_w - flint_px(10), row_h, font);
                if(ui_draw_icon_btn_padded(edit_x, y, icon_size, icon_padding,
                                           app->icons[UI_ICON_TYPE_SAVE], &hover)) {
                    habit_session_commit_edit(app, entry);
                    return y;
                }
            } else {
                flint_text_draw(line, content_x + flint_px(12),
                                flint_ui_text_y(line, y, row_h, font),
                                font, c_text);
                if(ui_draw_icon_btn_padded(edit_x, y, icon_size, icon_padding,
                                           app->icons[UI_ICON_TYPE_PENCIL], &hover)) {
                    app->habit_detail_session_index = i;
                    habit_session_begin_edit_time(app, entry);
                }
            }
            if(ui_draw_icon_btn_padded(right_x, y, icon_size, icon_padding,
                                       app->icons[UI_ICON_TYPE_TRASH], &hover)) {
                data_delete_session(entry->path);
                habit_session_cancel_edit(app);
                app->habit_detail_session_index = -1;
                return y;
            }
        }
        y += row_h + flint_px(4);

        for(int r = 0; r < entry->round_count; r++) {
            char round_line[64];
            int edit_round = habit_session_edit_matches(app, entry,
                                                        HABIT_SESSION_EDIT_ROUND, r);
            snprintf(round_line, sizeof(round_line), "Round %d  %ds",
                     r + 1, entry->rounds[r]);
            if(draw) {
                DrawRectangle(content_x + flint_px(14), y,
                              content_w - flint_px(14), row_h,
                              flint_darken(c_bg, 2));
                if(edit_round) {
                    habit_session_draw_edit_field(app, entry,
                                                  content_x + flint_px(26), y,
                                                  field_w - flint_px(26), row_h, font);
                    if(ui_draw_icon_btn_padded(edit_x, y, icon_size, icon_padding,
                                               app->icons[UI_ICON_TYPE_SAVE], &hover)) {
                        habit_session_commit_edit(app, entry);
                        return y;
                    }
                } else {
                    flint_text_draw(round_line, content_x + flint_px(28),
                                    flint_ui_text_y(round_line, y, row_h, font),
                                    font, c_text);
                    if(ui_draw_icon_btn_padded(edit_x, y, icon_size, icon_padding,
                                               app->icons[UI_ICON_TYPE_PENCIL], &hover)) {
                        app->habit_detail_session_index = i;
                        habit_session_begin_edit_round(app, entry, r);
                    }
                }
                if(ui_draw_icon_btn_padded(right_x, y, icon_size, icon_padding,
                                           app->icons[UI_ICON_TYPE_TRASH], &hover)) {
                    habit_session_delete_round(entry, r);
                    habit_session_cancel_edit(app);
                    return y;
                }
            }
            y += row_h + flint_px(4);
        }
        y += gap;
    }

    return y;
}

static void
draw_habit_session_edit_screen(InbeApp *app)
{
    HabitLinkedContext ctx;
    InbeHabit *habit;
    char date_text[32];
    int top_h = flint_px(58);
    int nav_h = flint_px(TAB_BAR_H);
    int keyboard_h;
    int viewport_h;
    int content_x;
    int content_w;
    int max_w = flint_px(CONTENT_MAX_W);
    int side_padding = flint_page_side_padding();
    int y = top_h + flint_px(14);
    int content_h;
    FlintUIHeader header;
    FlintUIScrollArea scroll_area;
    FlintUIScrollView scroll_view;

    if(app == NULL)
        return;
    if(app->habit_detail_index < 0 || app->habit_detail_index >= app->habits.count) {
        app->inbe.screen = InbeScreenHabits;
        return;
    }

    habit = &app->habits.items[app->habit_detail_index];
    habit_collect_linked_entries(habit, app->habit_detail_day, &ctx);
    habit_format_date(app->habit_detail_day, date_text, sizeof(date_text));

    keyboard_h = habit_session_keyboard_height(app);
    viewport_h = view_height - top_h - nav_h - keyboard_h;
    if(viewport_h < flint_px(80))
        viewport_h = flint_px(80);

    header = ui_draw_title_header(top_h, date_text,
                                  app->icons[UI_ICON_TYPE_RETURN], (Texture2D){0});
    if(header.left_clicked) {
        habit_session_cancel_edit(app);
        app->inbe.screen = InbeScreenHabits;
        return;
    }

    flint_centered_column(max_w, side_padding, &content_x, &content_w);
    content_h = draw_habit_session_edit_content(app, &ctx, content_x, content_w, y, 0) - y;
    scroll_area = (FlintUIScrollArea){
        .bounds = {(float)content_x, (float)y, (float)content_w, (float)viewport_h},
        .content_height = content_h,
        .scroll_offset = &app->habit_session_edit_scroll,
        .wheel_step = flint_px(42)
    };
    scroll_view = ui_scroll_container_begin(scroll_area);
    draw_habit_session_edit_content(app, &ctx, scroll_view.content_x, scroll_view.content_w,
                                    scroll_view.content_y, 1);
    ui_scroll_container_end(scroll_area, scroll_view);

    if(app->habit_session_edit_active) {
        HabitLinkedEntry *active_entry = NULL;
        for(int i = 0; i < ctx.count; i++) {
            if(strcmp(app->habit_session_edit_path, ctx.entries[i].path) == 0) {
                active_entry = &ctx.entries[i];
                break;
            }
        }
        if(active_entry != NULL)
            habit_session_draw_keyboard(app, active_entry);
    }
}

static int
draw_habits_linked_view(InbeApp *app, InbeHabit *habit, int content_x, int content_w, int y, int draw)
{
    HabitLinkedContext ctx;
    int row_h = flint_px(34);
    int current_year = -1;
    int current_month = -1;
    int current_day = -1;
    int selected = app != NULL ? app->habits.selected : 0;

    if(app == NULL || habit == NULL)
        return y;

    if(!habit_is_linked(habit)) {
        if(habit->day_count <= 0) {
            if(draw)
                flint_text_draw("No journal entries yet", content_x, y, flint_ui_font(), c_text);
            return y + row_h;
        }
        for(int i = habit->day_count - 1; i >= 0; i--) {
            char label[64];
            int day_index = habit->days[i].day_index;
            int year = day_index / 10000;
            int month = (day_index / 100) % 100;
            int day = day_index % 100;

            if(!habit->days[i].completed)
                continue;
            if(year != current_year) {
                snprintf(label, sizeof(label), "%04d", year);
                if(draw && draw_habit_cascade_row(app, content_x, y, content_w, row_h, label,
                                                  app->habits_list_expanded_year == year, 10)) {
                    app->habits_list_expanded_year = year;
                    app->habits_list_expanded_month = 0;
                    app->habits_list_expanded_day = 0;
                    app->habits_list_expanded_session = -1;
                    app->habits_list_scroll = 0;
                }
                y += row_h;
                current_year = year;
                current_month = -1;
            }
            if(app->habits_list_expanded_year != year)
                continue;
            if(month != current_month) {
                snprintf(label, sizeof(label), "%s", habit_month_label(month));
                if(draw && draw_habit_cascade_row(app, content_x, y, content_w, row_h, label,
                                                  app->habits_list_expanded_month == month, 22)) {
                    app->habits_list_expanded_month = month;
                    app->habits_list_expanded_day = 0;
                    app->habits_list_expanded_session = -1;
                    app->habits_list_scroll = 0;
                }
                y += row_h;
                current_month = month;
            }
            if(app->habits_list_expanded_month != month)
                continue;
            snprintf(label, sizeof(label), "%02d", day);
            if(draw && draw_habit_cascade_row(app, content_x, y, content_w, row_h, label,
                                              app->habits_list_expanded_day == day_index, 34)) {
                app->habits_list_expanded_day = day_index;
                app->habits_list_expanded_session = -1;
                app->habits_list_scroll = 0;
            }
            y += row_h;
            if(app->habits_list_expanded_day == day_index) {
                int hover = 0;
                if(draw && ui_draw_generic_button(content_x + flint_px(46), y,
                                                  content_w - flint_px(46), row_h,
                                                  "Mark incomplete", UI_BUTTON_STYLE_SECONDARY,
                                                  0, &hover))
                    inbe_habit_toggle_day(&app->habits, app->habits.selected, day_index);
                y += row_h;
            }
        }
        return y;
    }

    habit_collect_linked_entries(habit, 0, &ctx);
    if(ctx.count > 1)
        qsort(ctx.entries, (size_t)ctx.count, sizeof(ctx.entries[0]), compare_habit_linked_entries);
    if(ctx.count <= 0) {
        if(draw)
            flint_text_draw("No sessions yet", content_x, y, flint_ui_font(), c_text);
        return y + row_h;
    }

    if(app->habits_list_expanded_year == 0) {
        int today_index = inbe_habits_today_index();
        int opened = 0;
        for(int i = 0; i < ctx.count; i++) {
            if(habit_date_index(ctx.entries[i].year, ctx.entries[i].month, ctx.entries[i].day) == today_index) {
                app->habits_list_expanded_year = ctx.entries[i].year;
                app->habits_list_expanded_month = ctx.entries[i].month;
                app->habits_list_expanded_day = today_index;
                app->habits_list_expanded_session = i;
                opened = 1;
                break;
            }
        }
        if(!opened) {
            app->habits_list_expanded_year = ctx.entries[0].year;
            app->habits_list_expanded_month = ctx.entries[0].month;
            app->habits_list_expanded_day = habit_date_index(ctx.entries[0].year,
                                                             ctx.entries[0].month,
                                                             ctx.entries[0].day);
            app->habits_list_expanded_session = 0;
        }
    }

    for(int i = 0; i < ctx.count; i++) {
        char label[128];
        int day_index = habit_date_index(ctx.entries[i].year, ctx.entries[i].month, ctx.entries[i].day);

        if(ctx.entries[i].year != current_year) {
            snprintf(label, sizeof(label), "%04d", ctx.entries[i].year);
            if(draw && draw_habit_cascade_row(app, content_x, y, content_w, row_h, label,
                                              app->habits_list_expanded_year == ctx.entries[i].year, 10)) {
                app->habits_list_expanded_year = ctx.entries[i].year;
                app->habits_list_expanded_month = 0;
                app->habits_list_expanded_day = 0;
                app->habits_list_expanded_session = -1;
                app->habits_list_scroll = 0;
            }
            y += row_h;
            current_year = ctx.entries[i].year;
            current_month = -1;
            current_day = -1;
        }
        if(app->habits_list_expanded_year != ctx.entries[i].year)
            continue;
        if(ctx.entries[i].month != current_month) {
            snprintf(label, sizeof(label), "%s", habit_month_label(ctx.entries[i].month));
            if(draw && draw_habit_cascade_row(app, content_x, y, content_w, row_h, label,
                                              app->habits_list_expanded_month == ctx.entries[i].month, 22)) {
                app->habits_list_expanded_month = ctx.entries[i].month;
                app->habits_list_expanded_day = 0;
                app->habits_list_expanded_session = -1;
                app->habits_list_scroll = 0;
            }
            y += row_h;
            current_month = ctx.entries[i].month;
            current_day = -1;
        }
        if(app->habits_list_expanded_month != ctx.entries[i].month)
            continue;
        if(ctx.entries[i].day != current_day) {
            snprintf(label, sizeof(label), "%02d", ctx.entries[i].day);
            if(draw) {
                if(draw_habit_cascade_row(app, content_x, y, content_w, row_h, label,
                                          app->habits_list_expanded_day == day_index, 34)) {
                    app->habits_list_expanded_day = day_index;
                    app->habits_list_expanded_session = -1;
                    app->habits_list_scroll = 0;
                }
            }
            y += row_h;
            current_day = ctx.entries[i].day;
        }
        if(app->habits_list_expanded_day != day_index)
            continue;

        snprintf(label, sizeof(label), "%02d:%02d  %d rounds",
                 ctx.entries[i].hour, ctx.entries[i].minute,
                 ctx.entries[i].round_count);
        if(draw) {
            int row_w = content_w - flint_px(32);
            if(draw_habit_cascade_row(app, content_x, y, row_w, row_h, label,
                                      app->habits_list_expanded_session == i, 46)) {
                app->habits_list_expanded_session = i;
            }
            if(draw_habit_cascade_action(content_x + content_w, y, row_h,
                                         app->icons[UI_ICON_TYPE_PENCIL])) {
                habit_open_session_edit_page_for_session(app, selected, day_index,
                                                         ctx.entries[i].path);
                return y + row_h;
            }
        }
        y += row_h;

        if(app->habits_list_expanded_session == i) {
            for(int r = 0; r < ctx.entries[i].round_count; r++) {
                snprintf(label, sizeof(label), "Round %d  %ds", r + 1, ctx.entries[i].rounds[r]);
                if(draw)
                    draw_habit_cascade_row(app, content_x, y, content_w, row_h, label, 0, 10);
                y += row_h;
            }
        }
    }

    return y;
}

static void
draw_habits_screen(InbeApp *app)
{
    const char *title = locale_get("tab_habits");
    int top_h = flint_px(58);
    int nav_h = flint_px(TAB_BAR_H);
    int content_x;
    int content_w;
    int y = top_h + flint_px(16);
    int viewport_h = view_height - top_h - nav_h;
    int hover = 0;
    int side_padding = flint_page_side_padding();
    int max_w = flint_px(CONTENT_MAX_W);
    int selected_h = flint_px(44);
    int month_h = flint_px(42);
    int grid_gap = flint_px(4);
    int selected;
    InbeHabit *active;
    time_t now;
    struct tm month;
    struct tm next_month;
    char month_label[64];
    int year;
    int mon;
    int first_wday;
    int days_in_month;
    int today_index = inbe_habits_today_index();
    int cell_w;
    int cell_h;
    int grid_x;
    int grid_y;
    HabitLinkedContext *linked_ctx = NULL;
    int active_is_linked;
    int forward_disabled;

    draw_app_title_bar(title);
    draw_habit_view_button(app);
    draw_habits_manager_button(app);

    if(app == NULL)
        return;

    flint_centered_column(max_w, side_padding, &content_x, &content_w);

    if(app->habits.count <= 0) {
        const char *empty_text = "No habits created";
        const char *create_text = "Create habit";
        int empty_font = flint_px(20);
        int button_w = content_w < flint_px(240) ? content_w : flint_px(240);
        int button_h = flint_px(42);
        int empty_y = top_h + viewport_h / 2 - flint_px(46);
        int empty_w = flint_text_measure(empty_text, empty_font);
        int hover_empty_create = 0;

        ui_begin_scissor((int)app->camera.offset.x,
                         (int)(app->camera.offset.y + top_h * app->camera.zoom),
                         (int)(view_width * app->camera.zoom),
                         (int)(viewport_h * app->camera.zoom));
        flint_text_draw(empty_text, content_x + (content_w - empty_w) / 2,
                        empty_y, empty_font, c_text);
        if(ui_draw_generic_button(content_x + (content_w - button_w) / 2,
                                  empty_y + flint_px(38), button_w, button_h,
                                  create_text, UI_BUTTON_STYLE_PRIMARY,
                                  0, &hover_empty_create)) {
            habit_edit_begin_new(app);
        }
        ui_end_scissor();
        return;
    }
    if(app->habits.selected < 0 || app->habits.selected >= app->habits.count)
        app->habits.selected = 0;

    selected = app->habits.selected;
    active = &app->habits.items[selected];
    active_is_linked = habit_is_linked(active);

    if(app->habits.month_offset > 0)
        app->habits.month_offset = 0;

    now = time(NULL);
    {
        struct tm *local = localtime(&now);
        if(local != NULL)
            month = *local;
        else
            memset(&month, 0, sizeof(month));
    }
    month.tm_mday = 1;
    month.tm_hour = 12;
    month.tm_min = 0;
    month.tm_sec = 0;
    month.tm_mon += app->habits.month_offset;
    mktime(&month);

    year = month.tm_year + 1900;
    mon = month.tm_mon + 1;
    first_wday = month.tm_wday;
    next_month = month;
    next_month.tm_mon++;
    next_month.tm_mday = 0;
    mktime(&next_month);
    days_in_month = next_month.tm_mday;
    strftime(month_label, sizeof(month_label), "%B %Y", &month);

    ui_begin_scissor((int)app->camera.offset.x,
                     (int)(app->camera.offset.y + top_h * app->camera.zoom),
                     (int)(view_width * app->camera.zoom),
                     (int)(viewport_h * app->camera.zoom));

    if(app->habits.count > 1) {
        DrawRectangle(content_x, y, content_w, selected_h, flint_darken(c_bg, 6));
        ui_draw_bevel(content_x, y, content_w, selected_h,
                      flint_lighten(c_button, 24), flint_darken(c_button, 24));
        DrawRectangle(content_x, y, flint_px(6), selected_h, active->color);
        flint_text_draw(active->name,
                        content_x + flint_px(16),
                        flint_ui_text_y(active->name, y, selected_h, flint_ui_font()),
                        flint_ui_font(), c_text);

        y += selected_h + flint_px(8);
    }

    if(app->habits_view_mode == HABITS_VIEW_LINKED) {
        int list_y = y + flint_px(8);
        int list_view_h = top_h + viewport_h - list_y - flint_px(8);
        int list_content_h = draw_habits_linked_view(app, active, content_x, content_w, list_y, 0) - list_y;
        FlintUIScrollArea scroll_area;
        FlintUIScrollView scroll_view;

        if(list_view_h < 1)
            list_view_h = 1;
        ui_end_scissor();

        scroll_area = (FlintUIScrollArea){
            .bounds = {(float)content_x, (float)list_y, (float)content_w, (float)list_view_h},
            .content_height = list_content_h,
            .scroll_offset = &app->habits_list_scroll,
            .wheel_step = flint_px(42)
        };

        ui_set_input_blocked(app->modal.active);
        scroll_view = ui_scroll_container_begin(scroll_area);
        draw_habits_linked_view(app, active, scroll_view.content_x, scroll_view.content_w,
                                scroll_view.content_y, 1);
        ui_scroll_container_end(scroll_area, scroll_view);
        ui_set_input_blocked(0);
        return;
    }

    if(active_is_linked) {
        linked_ctx = calloc(1, sizeof(*linked_ctx));
        if(linked_ctx != NULL)
            habit_collect_linked_entries(active, 0, linked_ctx);
    }

    y += flint_px(10);

    forward_disabled = app->habits.month_offset >= 0;
    ui_set_input_blocked(app->modal.active);
    if(ui_draw_generic_button(content_x, y, flint_px(44), month_h, "<",
                              UI_BUTTON_STYLE_SECONDARY, 0, &hover)) {
        app->habits.month_offset--;
    }
    if(ui_draw_generic_button(content_x + content_w - flint_px(44), y,
                                     flint_px(44), month_h, ">",
                                     UI_BUTTON_STYLE_SECONDARY,
                                     forward_disabled, &hover)) {
        app->habits.month_offset++;
    }
    flint_text_draw(month_label,
                    content_x + (content_w - flint_text_measure(month_label, flint_px(22))) / 2,
                    flint_ui_text_y(month_label, y, month_h, flint_px(22)),
                    flint_px(22), c_text);

    grid_x = content_x;
    grid_y = y + month_h + flint_px(12);
    cell_w = (content_w - grid_gap * 6) / 7;
    cell_h = cell_w;
    {
        int available_h = top_h + viewport_h - grid_y - flint_px(8);
        int max_cell_h = (available_h - grid_gap * 5) / 6;
        if(cell_h > max_cell_h)
            cell_h = max_cell_h;
        if(cell_h < flint_px(28))
            cell_h = flint_px(28);
    }

    for(int row = 0; row < 6; row++) {
        for(int col = 0; col < 7; col++) {
            int slot = row * 7 + col;
            int day = slot - first_wday + 1;
            int cell_x = grid_x + col * (cell_w + grid_gap);
            int cell_y = grid_y + row * (cell_h + grid_gap);
            char day_label[16];
            int day_index;
            int completed;
            int future_day;

            if(day < 1 || day > days_in_month) {
                DrawRectangle(cell_x, cell_y, cell_w, cell_h, flint_darken(c_bg, 5));
                continue;
            }

            snprintf(day_label, sizeof(day_label), "%d", day);
            day_index = year * 10000 + mon * 100 + day;
            future_day = day_index > today_index;
            completed = inbe_habit_completed_day(active, day_index);
            if(ui_draw_generic_button(cell_x, cell_y, cell_w, cell_h, day_label,
                                             completed ? UI_BUTTON_STYLE_PRIMARY : UI_BUTTON_STYLE_SECONDARY,
                                             future_day, &hover)) {
                if(active_is_linked) {
                    habit_open_linked_details(app, selected, day_index);
                } else {
                    inbe_habit_toggle_day(&app->habits, selected, day_index);
                    active = &app->habits.items[selected];
                }
            }
            if(completed && !future_day) {
                DrawRectangle(cell_x + flint_px(4), cell_y + cell_h - flint_px(6),
                              cell_w - flint_px(8), flint_px(3), active->color);
            }
            if(!future_day && linked_ctx != NULL && habit_linked_has_day(linked_ctx, day_index)) {
                DrawCircle(cell_x + cell_w - flint_px(8), cell_y + flint_px(8),
                           flint_px(3), active->color);
            }
            if(day_index == today_index) {
                DrawRectangleLinesEx((Rectangle){(float)cell_x, (float)cell_y,
                                                 (float)cell_w, (float)cell_h},
                                     (float)flint_px(2), c_text);
            }
        }
    }

    ui_end_scissor();
    free(linked_ctx);
    ui_set_input_blocked(0);
    draw_habit_linked_details_modal(app);
}

static void
draw_habits_manager_screen(InbeApp *app)
{
    const char *title = "Habits";
    int top_h = flint_px(58);
    int nav_h = flint_px(TAB_BAR_H);
    int side_padding = flint_page_side_padding();
    int max_w = flint_px(CONTENT_MAX_W);
    int content_x;
    int content_w;
    int viewport_h = view_height - top_h - nav_h;
    int y;
    int hover = 0;
    int icon_size = flint_px(22);
    int icon_padding = flint_px(8);
    int icon_btn_w = icon_size + icon_padding * 2;
    int row_h = flint_px(46);
    int row_gap = flint_px(8);
    int font = flint_ui_font();
    int plus_size = flint_px(24);
    int plus_padding = flint_px(12);
    int plus_btn_w = plus_size + plus_padding * 2;
    int plus_y;
    int plus_x;
    int title_font = flint_px(22);
    int title_w;
    int pending_delete = -1;

    if(app == NULL)
        return;

    DrawRectangle(0, 0, view_width, top_h, c_bg);
    DrawLine(0, top_h - 1, view_width, top_h - 1, flint_darken(c_button, 18));

    if(ui_draw_icon_btn_padded(flint_px(12), flint_px(12), flint_px(24),
                               flint_px(8), app->icons[UI_ICON_TYPE_RETURN], &hover)) {
        habit_edit_commit(app);
        app->inbe.screen = InbeScreenHabits;
        return;
    }

    title_w = flint_text_measure(title, title_font);
    flint_text_draw(title, (view_width - title_w) / 2,
                    flint_ui_text_y(title, 0, top_h, title_font),
                    title_font, c_text);

    flint_centered_column(max_w, side_padding, &content_x, &content_w);
    y = top_h + flint_px(16);

    if(app->habits.selected < 0 || app->habits.selected >= app->habits.count)
        app->habits.selected = 0;
    if(app->habit_edit_active && app->habit_edit_index >= app->habits.count)
        habit_edit_cancel(app);

    ui_begin_scissor((int)app->camera.offset.x,
                     (int)(app->camera.offset.y + top_h * app->camera.zoom),
                     (int)(view_width * app->camera.zoom),
                     (int)(viewport_h * app->camera.zoom));

    if(app->habits.count <= 0) {
        const char *empty_text = "No habits created";
        const char *defaults_text = "Create default habits";
        int empty_font = flint_px(20);
        int button_w = content_w < flint_px(260) ? content_w : flint_px(260);
        int button_h = flint_px(42);
        int empty_y = top_h + viewport_h / 2 - flint_px(50);
        int empty_w = flint_text_measure(empty_text, empty_font);
        int hover_defaults = 0;

        flint_text_draw(empty_text, content_x + (content_w - empty_w) / 2,
                        empty_y, empty_font, c_text);
        if(ui_draw_generic_button(content_x + (content_w - button_w) / 2,
                                  empty_y + flint_px(38), button_w, button_h,
                                  defaults_text, UI_BUTTON_STYLE_PRIMARY,
                                  0, &hover_defaults)) {
            inbe_habits_add_default_set(&app->habits);
            ui_end_scissor();
            return;
        }
    }

    for(int i = 0; i < app->habits.count; i++) {
        InbeHabit *habit = &app->habits.items[i];
        int row_x = content_x;
        int row_w = content_w;
        int field_x = row_x + flint_px(18);
        int delete_disabled = 0;
        int delete_btn_x = row_x + row_w - icon_btn_w;
        int edit_btn_x = delete_btn_x - icon_btn_w - flint_px(6);
        Rectangle row = {(float)row_x, (float)y, (float)row_w, (float)row_h};
        Rectangle delete_bounds = {
            (float)delete_btn_x,
            (float)(y + (row_h - icon_btn_w) / 2),
            (float)icon_btn_w,
            (float)icon_btn_w
        };
        Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
        int row_hover = CheckCollisionPointRec(mouse_world, row);

        DrawRectangle(row_x, y, row_w, row_h,
                      i == app->habits.selected ? flint_lighten(c_button, 8) : flint_darken(c_bg, 5));
        DrawRectangleLinesEx(row, (float)flint_px(1),
                             row_hover ? c_button_hover : flint_darken(c_button, 14));
        DrawRectangle(row_x, y, flint_px(6), row_h, habit->color);

        if(row_hover) {
            app->cursor_clickable = 1;
            if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                app->habits.selected = i;
                inbe_habits_save(&app->habits);
            }
        }

        flint_text_draw(habit->name,
                        field_x,
                        flint_ui_text_y(habit->name, y, row_h, font),
                        font, c_text);
        if(ui_draw_icon_btn_padded(edit_btn_x, y + (row_h - icon_btn_w) / 2,
                                   icon_size, icon_padding, app->icons[UI_ICON_TYPE_PENCIL], &hover)) {
            app->habits.selected = i;
            habit_edit_begin(app, i);
        }

        if(delete_disabled && CheckCollisionPointRec(mouse_world, delete_bounds))
            app->cursor_disabled = 1;
        if(flint_ui_icon_button((FlintUIIconButton){
                .bounds = delete_bounds,
                .icon = app->icons[UI_ICON_TYPE_TRASH],
                .icon_type = UI_ICON_TYPE_TRASH,
                .icon_size = icon_size,
                .icon_padding = icon_padding,
                .disabled = delete_disabled,
                .background = flint_darken(c_button, delete_disabled ? 16 : 6),
                .hover_background = flint_darken(c_button_hover, 8),
                .icon_color = delete_disabled ? flint_darken(c_text, 90) : c_icon,
                .border = flint_darken(c_button, 24),
                .radius = 0.12f
            })) {
            pending_delete = i;
        }

        y += row_h + row_gap;
    }

    if(pending_delete >= 0) {
        habit_edit_cancel(app);
        inbe_habits_delete(&app->habits, pending_delete);
        ui_end_scissor();
        return;
    }

    plus_x = content_x + content_w - plus_btn_w;
    plus_y = view_height - nav_h - plus_btn_w - flint_px(16);
    if(plus_y < y + flint_px(8))
        plus_y = y + flint_px(8);
    if(ui_draw_icon_btn_padded(plus_x, plus_y, plus_size, plus_padding,
                               app->icons[UI_ICON_TYPE_PLUS], &hover)) {
        if(app->habits.count < INBE_HABIT_MAX)
            habit_edit_begin_new(app);
    }

    ui_end_scissor();
}

static int
habit_activity_index(int exercise)
{
    static const int activities[] = {
        EXERCISE_WIM_HOF,
        EXERCISE_MEDITATION,
        EXERCISE_SUN_SALUTATION,
        EXERCISE_7_MINUTE_WORKOUT
    };

    for(int i = 0; i < (int)(sizeof(activities) / sizeof(activities[0])); i++) {
        if(activities[i] == exercise)
            return i;
    }
    return 0;
}

static int
habit_activity_from_index(int index)
{
    static const int activities[] = {
        EXERCISE_WIM_HOF,
        EXERCISE_MEDITATION,
        EXERCISE_SUN_SALUTATION,
        EXERCISE_7_MINUTE_WORKOUT
    };
    int count = (int)(sizeof(activities) / sizeof(activities[0]));

    index = clampi(index, 0, count - 1);
    return activities[index];
}

static int
habit_color_button(InbeApp *app, int x, int y, Color color, int selected)
{
    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
    int radius = flint_px(13);
    Rectangle bounds = {
        (float)(x - radius - flint_px(6)),
        (float)(y - radius - flint_px(6)),
        (float)(radius * 2 + flint_px(12)),
        (float)(radius * 2 + flint_px(12))
    };
    int hovered = CheckCollisionPointRec(mouse_world, bounds);

    DrawCircle(x, y, radius, color);
    DrawCircleLines(x, y, radius + flint_px(2),
                    selected ? c_text : flint_darken(c_bg, 42));
    if(hovered) {
        app->cursor_clickable = 1;
        DrawCircleLines(x, y, radius + flint_px(5), c_button_hover);
        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
            return 1;
    }
    return 0;
}

static void
draw_habit_edit_screen(InbeApp *app)
{
    const char *title;
    const char *sync_options[] = {"None", "Topic", "Activity"};
    const char *topic_options[] = {"Mind", "Yoga", "Fitness"};
    const char *activity_options[] = {
        "Wim Hof Breathing",
        "Meditation",
        "Sun Salutation",
        "7-Minute Workout"
    };
    Color color_options[6];
    int top_h = flint_px(58);
    int nav_h = flint_px(TAB_BAR_H);
    int content_x;
    int content_w;
    int max_w = flint_px(CONTENT_MAX_W);
    int y = top_h + flint_px(18);
    int font = flint_ui_font();
    int label_font = flint_ui_font_small();
    int field_h = flint_px(40);
    int hover = 0;
    int title_font = flint_px(22);
    int title_w;
    int sync_mode;
    int topic_index;
    int activity_index;
    int dropdown_changed = 0;

    if(app == NULL)
        return;

    if(!app->habit_edit_active) {
        app->inbe.screen = InbeScreenHabitManager;
        return;
    }

    title = app->habit_edit_is_new ? "New Habit" : "Edit Habit";

    DrawRectangle(0, 0, view_width, top_h, c_bg);
    DrawLine(0, top_h - 1, view_width, top_h - 1, flint_darken(c_button, 18));
    if(ui_draw_icon_btn_padded(flint_px(12), flint_px(12), flint_px(24),
                               flint_px(8), app->icons[UI_ICON_TYPE_RETURN], &hover)) {
        habit_edit_commit(app);
        return;
    }
    title_w = flint_text_measure(title, title_font);
    flint_text_draw(title, (view_width - title_w) / 2,
                    flint_ui_text_y(title, 0, top_h, title_font),
                    title_font, c_text);
    if(ui_draw_icon_btn_padded(view_width - flint_px(52), flint_px(12),
                               flint_px(24), flint_px(8), app->icons[UI_ICON_TYPE_SAVE], &hover)) {
        habit_edit_commit(app);
        return;
    }

    flint_centered_column(max_w, flint_page_side_padding(), &content_x, &content_w);

    ui_begin_scissor((int)app->camera.offset.x,
                     (int)(app->camera.offset.y + top_h * app->camera.zoom),
                     (int)(view_width * app->camera.zoom),
                     (int)((view_height - top_h - nav_h) * app->camera.zoom));

    flint_text_draw("Name", content_x, y, label_font, flint_darken(c_text, 34));
    y += flint_px(22);
    draw_habit_edit_field(app, content_x, y, content_w, field_h, font);
    habit_edit_update_input(app, content_x, y, content_w, field_h,
                            content_x + flint_px(10), font);
    y += field_h + flint_px(24);

    flint_text_draw("Color", content_x, y, label_font, flint_darken(c_text, 34));
    y += flint_px(32);
    color_options[0] = practice_theme_color(app, PRACTICE_CATEGORY_MIND);
    color_options[1] = practice_theme_color(app, PRACTICE_CATEGORY_YOGA);
    color_options[2] = practice_theme_color(app, PRACTICE_CATEGORY_FITNESS);
    color_options[3] = (Color){99, 196, 165, 255};
    color_options[4] = (Color){210, 180, 72, 255};
    color_options[5] = (Color){180, 132, 220, 255};
    for(int i = 0; i < 6; i++) {
        int cx = content_x + flint_px(18) + i * flint_px(42);
        int selected = app->habit_edit_color.r == color_options[i].r &&
                       app->habit_edit_color.g == color_options[i].g &&
                       app->habit_edit_color.b == color_options[i].b;
        if(habit_color_button(app, cx, y, color_options[i], selected))
            app->habit_edit_color = color_options[i];
    }
    y += flint_px(34);

    flint_text_draw("Link", content_x, y, label_font, flint_darken(c_text, 34));
    y += flint_px(24);
    sync_mode = clampi(app->habit_edit_sync_mode, INBE_HABIT_SYNC_NONE, INBE_HABIT_SYNC_ACTIVITY);
    {
        int gap = flint_px(8);
        int btn_w = (content_w - gap * 2) / 3;
        for(int i = 0; i < 3; i++) {
            if(ui_draw_generic_button(content_x + i * (btn_w + gap), y,
                                      btn_w, flint_px(36), sync_options[i],
                                      sync_mode == i ? UI_BUTTON_STYLE_PRIMARY : UI_BUTTON_STYLE_SECONDARY,
                                      0, &hover)) {
                app->habit_edit_sync_mode = i;
                sync_mode = i;
            }
        }
    }
    y += flint_px(48);

    if(sync_mode == INBE_HABIT_SYNC_TOPIC) {
        flint_text_draw("Topic", content_x, y, label_font, flint_darken(c_text, 34));
        y += flint_px(22);
        topic_index = clampi(app->habit_edit_sync_topic, 0, INBE_HABIT_TOPIC_COUNT - 1);
        if(ui_draw_dropdown_button(610, content_x, y, content_w, field_h,
                                   topic_options, 3, &topic_index)) {
            app->habit_edit_sync_topic = topic_index;
            app->habit_edit_color = practice_theme_color(app, topic_index);
        }
        dropdown_changed = ui_draw_dropdown_menu(610);
        if(dropdown_changed) {
            app->habit_edit_sync_topic = topic_index;
            app->habit_edit_color = practice_theme_color(app, topic_index);
        }
        y += field_h + flint_px(18);
    } else if(sync_mode == INBE_HABIT_SYNC_ACTIVITY) {
        flint_text_draw("Activity", content_x, y, label_font, flint_darken(c_text, 34));
        y += flint_px(22);
        activity_index = habit_activity_index(app->habit_edit_sync_activity);
        if(ui_draw_dropdown_button(611, content_x, y, content_w, field_h,
                                   activity_options, 4, &activity_index)) {
            app->habit_edit_sync_activity = habit_activity_from_index(activity_index);
        }
        dropdown_changed = ui_draw_dropdown_menu(611);
        if(dropdown_changed)
            app->habit_edit_sync_activity = habit_activity_from_index(activity_index);
        y += field_h + flint_px(18);
    }

    ui_end_scissor();
}

static void
updateapp(InbeApp *app)
{
    int center_x = view_width / 2;
    int center_y = view_height / 2;
    int hover = 0;

    app_apply_pending_bottom_tab(app);
    meditation_music_update(app);
    if(app->practice_coming_soon_ticks > 0)
        app->practice_coming_soon_ticks--;

    /* Handle Android back button and desktop backspace */
    if(IsKeyPressed(KEY_BACK) ||
       (IsKeyPressed(KEY_BACKSPACE) &&
        !(app->inbe.screen == InbeScreenHabitEdit && app->habit_edit_active) &&
        !(app->inbe.screen == InbeScreenHabitSessionEdit && app->habit_session_edit_active))) {
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
        app_draw_bottom_nav(app);
        goto finish_frame;
    }

    if(app->inbe.screen == InbeScreenLanguage) {
        language_tab_draw(app);
        goto finish_frame;
    }

    if(app->inbe.screen == InbeScreenManual) {
        manual_tab_draw(app);
        goto finish_frame;
    }

    if(app->inbe.screen == InbeScreenHabits) {
        draw_habits_screen(app);
        app_draw_bottom_nav(app);
        goto finish_frame;
    }

    if(app->inbe.screen == InbeScreenHabitManager) {
        draw_habits_manager_screen(app);
        app_draw_bottom_nav(app);
        goto finish_frame;
    }

    if(app->inbe.screen == InbeScreenHabitEdit) {
        draw_habit_edit_screen(app);
        app_draw_bottom_nav(app);
        goto finish_frame;
    }

    if(app->inbe.screen == InbeScreenHabitSessionEdit) {
        draw_habit_session_edit_screen(app);
        app_draw_bottom_nav(app);
        goto finish_frame;
    }

    if(app->inbe.screen == InbeScreenPracticeConfig) {
        draw_practice_config_page(app);
        app_draw_bottom_nav(app);
        goto finish_frame;
    }

    if(app->inbe.screen == InbeScreenStart) {
        session_update_circle_bounds_for_view(&app->inbe, flint_px(48),
                                      practice_category_bottom_y_for_app(app) + flint_px(96));
    } else if(app->inbe.screen == InbeScreenSession) {
        session_update_circle_bounds_for_view(&app->inbe, 0, 84);
    }

    int play_circle_clicked = 0;
    if(app->inbe.screen == InbeScreenStart)
        play_circle_clicked = session_draw_start_preview(app, center_x, center_y);
    else if(app->inbe.screen == InbeScreenSession)
        session_draw_inbe(app, center_x, center_y);


    switch (app->inbe.screen) {
    case InbeScreenStart:
        draw_app_title_bar(locale_get("tab_practice"));
        draw_practice_config_button(app);
        draw_practice_category_tabs(app);

        {
            const char *exercise_options[2];
            int exercise_values[2];
            int activity_count;
            int activity_index;
            int dropdown_w = flint_px(250);
            int dropdown_h = flint_px(36);
            int manual_icon_size = flint_px(20);
            int manual_icon_padding = flint_px(8);
            int manual_btn_w = manual_icon_size + manual_icon_padding * 2;
            int settings_icon_size = flint_px(20);
            int settings_icon_padding = flint_px(8);
            int settings_btn_w = settings_icon_size + settings_icon_padding * 2;
            int selector_gap = flint_px(8);
            int selector_w;
            int dropdown_x;
            int dropdown_y;
            int manual_x;
            int settings_x;
            int exercise_changed = 0;

            practice_clamp_activity_to_tab(app);
            activity_count = practice_activity_count_for_tab(app->practice_category_tab);
            for(int i = 0; i < activity_count; i++) {
                exercise_values[i] = practice_activity_for_tab(app->practice_category_tab, i);
                exercise_options[i] = practice_activity_label(exercise_values[i]);
            }
            activity_index = practice_activity_index_for_tab(app->practice_category_tab, app->exercise_type);

            if(dropdown_w + selector_gap * 2 + manual_btn_w + settings_btn_w > view_width - flint_px(32))
                dropdown_w = view_width - flint_px(32) - selector_gap * 2 - manual_btn_w - settings_btn_w;
            if(dropdown_w < flint_px(160))
                dropdown_w = flint_px(160);
            selector_w = dropdown_w + selector_gap * 2 + manual_btn_w + settings_btn_w;
            dropdown_x = center_x - selector_w / 2;
            manual_x = dropdown_x + dropdown_w + selector_gap;
            settings_x = manual_x + manual_btn_w + selector_gap;
            if(practice_enabled_count(app) <= 1) {
                dropdown_y = practice_category_bottom_y_for_app(app) + flint_px(14);
            } else {
                dropdown_y = center_y - (int)(app->inbe.rmax * 0.72f) - flint_px(70);
                if(dropdown_y < practice_category_bottom_y_for_app(app) + flint_px(14))
                    dropdown_y = practice_category_bottom_y_for_app(app) + flint_px(14);
            }

            if(!app->modal.active &&
               ui_draw_dropdown_button(300, dropdown_x, dropdown_y, dropdown_w, dropdown_h,
                                       exercise_options, activity_count, &activity_index)) {
                app->exercise_type = exercise_values[activity_index];
                exercise_changed = 1;
            }
            if(!app->modal.active &&
               ui_draw_icon_btn_padded(manual_x, dropdown_y, manual_icon_size, manual_icon_padding,
                                       app->icons[UI_ICON_TYPE_MANUAL], &hover)) {
                app->tutorial_step = 0;
                app->manual_scroll = 0;
                app->inbe.screen = InbeScreenManual;
            }
            if(!app->modal.active &&
               ui_draw_icon_btn_padded(settings_x, dropdown_y, settings_icon_size, settings_icon_padding,
                                       app->icons[UI_ICON_TYPE_GEAR], &hover)) {
                reset_settings_preview(app);
                app->settings_category = app->exercise_type == EXERCISE_MEDITATION
                                             ? SETTINGS_CATEGORY_MEDITATION
                                             : SETTINGS_CATEGORY_PRACTICE;
                app->settings_sub_tab = app->settings_category == SETTINGS_CATEGORY_PRACTICE
                                            ? PRACTICE_SUBTAB_BREATHING
                                            : 0;
                app->settings_from_exercise_selector = 1;
                app->settings_scroll = 0;
                app->inbe.screen = InbeScreenSettings;
            }

            if(!app->modal.active && play_circle_clicked) {
                if(!exercise_manual_seen(app, app->exercise_type)) {
                    app->tutorial_step = 0;
                    app->manual_scroll = 0;
                    app->inbe.screen = InbeScreenManual;
                } else if(app->exercise_type == EXERCISE_MEDITATION) {
                    app->modal.active = 1;
                    app->modal.type = UIModalMeditationSetup;
                    app->modal.selected_button = 0;
                } else {
                    session_start(app);
                }
            }

            if(!app->modal.active && ui_draw_dropdown_menu(300)) {
                app->exercise_type = exercise_values[activity_index];
                exercise_changed = 1;
            }
            if(exercise_changed)
                save_settings(app);
        }
        draw_practice_coming_soon_popout(app);
        app_draw_bottom_nav(app);
        if(app->modal.active && app->modal.type == UIModalMeditationSetup)
            draw_meditation_setup_modal(app);
        break;

    case InbeScreenSession:
        session_update_screen(app, center_x, center_y, &hover);
        break;

    case InbeScreenMeditation:
        draw_meditation_screen(app, center_x, center_y);
        break;

    case InbeScreenResults:
        session_draw_results_screen(app, center_x, center_y, &hover);
        break;

    }

finish_frame:
    app_flush_deferred_settings(app);
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
    app->cursor_disabled = 0;
    app->camera.zoom = 1.0f;
    app->camera.offset.x = viewport.x;
    app->camera.offset.y = viewport.y;
    ui_set_frame(app->camera);
    ui_set_cursor_clickable(&app->cursor_clickable);
    ui_set_cursor_disabled(&app->cursor_disabled);
    app_device_preferences_update(app);
    app_refresh_theme(app);

    DrawRectangleRec(viewport, c_bg);
    ui_begin_scissor((int)viewport.x, (int)viewport.y, (int)viewport.width, (int)viewport.height);
        BeginMode2D(app->camera);
            DrawRectangle(0, 0, view_width, view_height, c_bg);
            updateapp(app);
        EndMode2D();
    ui_end_scissor();
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

    SafeUnloadTexture(app->icons[UI_ICON_TYPE_GEAR]);
    SafeUnloadTexture(app->icons[UI_ICON_TYPE_X]);
    SafeUnloadTexture(app->icons[UI_ICON_TYPE_MANUAL]);
    SafeUnloadTexture(app->icons[UI_ICON_TYPE_RETURN]);
    SafeUnloadTexture(app->icons[UI_ICON_TYPE_BACKWARD]);
    SafeUnloadTexture(app->icons[UI_ICON_TYPE_FORWARD]);
    SafeUnloadTexture(app->icons[UI_ICON_TYPE_PLAY]);
    SafeUnloadTexture(app->icons[UI_ICON_TYPE_PAUSE]);
    SafeUnloadTexture(app->icons[UI_ICON_TYPE_STAT]);
    SafeUnloadTexture(app->icons[UI_ICON_TYPE_HABIT]);
    SafeUnloadTexture(app->icons[UI_ICON_TYPE_AMEN]);
    SafeUnloadTexture(app->icons[UI_ICON_TYPE_PLUS]);
    SafeUnloadTexture(app->icons[UI_ICON_TYPE_STACK]);
    SafeUnloadTexture(app->icons[UI_ICON_TYPE_HOME]);
    SafeUnloadTexture(app->icons[UI_ICON_TYPE_TRASH]);
    SafeUnloadTexture(app->icons[UI_ICON_TYPE_PENCIL]);
    SafeUnloadTexture(app->icons[UI_ICON_TYPE_SAVE]);
    SafeUnloadTexture(app->icons[UI_ICON_TYPE_DISCORD]);
    SafeUnloadTexture(app->icons[UI_ICON_TYPE_TELEGRAM]);
    SafeUnloadTexture(app->icons[UI_ICON_TYPE_GITHUB]);
    SafeUnloadTexture(app->icons[UI_ICON_TYPE_BTC]);
    SafeUnloadTexture(app->icons[UI_ICON_TYPE_MONERO]);
    SafeUnloadTexture(app->icons[UI_ICON_TYPE_SOUND0]);
    SafeUnloadTexture(app->icons[UI_ICON_TYPE_SOUND1]);
    SafeUnloadTexture(app->icons[UI_ICON_TYPE_SOUND2]);
    SafeUnloadTexture(app->icons[UI_ICON_TYPE_SOUND3]);
    SafeUnloadTexture(app->whm_1_image);
    SafeUnloadTexture(app->whm_2_image);
    SafeUnloadTexture(app->font_shapes_texture);
    unload_locale_font(app);

    SafeUnloadSound(app->breath_in_sound);
    SafeUnloadSound(app->breath_out_sound);
    SafeUnloadSound(app->bell_sound);
    meditation_music_unload(app);

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
