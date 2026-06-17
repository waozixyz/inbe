#include "app.h"
#include "data.h"
#include "locale.h"
#include "screens/language_screen.h"
#include "screens/manual_screen.h"
#include "screens/settings/settings_screen.h"
#include "screens/practice_screen.h"
#include "session.h"
#include "device_preferences.h"
#include "meditation_music.h"
#include "storage.h"
#include "theme.h"
#if defined(LOTUS_BUILD)
#include "lotus_settings.h"
#endif
#include "version.h"
#include "flint_clip.h"
#include "flint_ui.h"
#include "flint_dpi.h"
#include "flint_text.h"
#include "flint_embedded_assets.h"

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
#include "android_device.h"
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
/* Theme colors are now accessed via theme accessor functions */

#define LOCALE_FONT_PNG "assets/fonts/locales.png"
#define LOCALE_FONT_8_PNG "assets/fonts/locales-8.png"

#define LOCALE_FONT_DAT "assets/fonts/locales.dat"
#define LOCALE_FONT_8_DAT "assets/fonts/locales-8.dat"
#define LOCALE_FONT_BASE_SIZE 16
#define LOCALE_FONT_8_BASE_SIZE 8


/* Forward declarations for tab callbacks */
void reset_settings_preview(InbeApp *app);

static void
app_leave_practice_config(InbeApp *app)
{
    if(app == NULL)
        return;
    if(app->settings_dirty)
        save_settings(app);
    meditation_music_unload(app);
    app->settings_scroll = 0;
}

static void
app_open_main_tab(InbeApp *app, int main_tab, int persist)
{
    if(app == NULL)
        return;

    if(app->inbe.screen == InbeScreenPracticeConfig)
        app_leave_practice_config(app);

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

void
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
        app_open_main_tab(app, APP_MAIN_TAB_HABITS, 1);
        break;
    case APP_BOTTOM_TAB_PRACTICE:
        app_open_main_tab(app, APP_MAIN_TAB_PRACTICE, 1);
        break;
    case APP_BOTTOM_TAB_SETTINGS:
        if(app->inbe.screen == InbeScreenPracticeConfig)
            app_leave_practice_config(app);
        reset_settings_preview(app);
        app->settings_tab = SETTINGS_TAB_DEVICE;
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
    case InbeScreenPracticeConfig:
    case InbeScreenHabits:
    case InbeScreenHabitEdit:
        return !app->habit_session_edit_active;
    case InbeScreenHabitSessionEdit:
        return 0;
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

static void on_settings_screen_click(void *user_data) {
    InbeApp *app = user_data;
    app_request_bottom_tab(app, APP_BOTTOM_TAB_SETTINGS);
}

static UITab g_tabs[] = {
    {NULL, {0}, UI_ICON_TYPE_HOME, on_habits_tab_click, NULL},
    {NULL, {0}, UI_ICON_TYPE_AMEN, NULL, NULL},
    {NULL, {0}, UI_ICON_TYPE_GEAR, on_settings_screen_click, NULL}
};

static UITabBar g_tab_bar = {g_tabs, 3};

static void
app_draw_bottom_nav(InbeApp *app)
{
    if(app_should_draw_bottom_nav(app))
        ui_draw_tab_bar(g_tab_bar.tabs, g_tab_bar.count);
}

static int
load_locale_font(InbeApp *app)
{
    Font font;
    Font font_8;
    Image white;
    const FlintEmbeddedAsset *png;
    const FlintEmbeddedAsset *dat;
    const FlintEmbeddedAsset *png_8;
    const FlintEmbeddedAsset *dat_8;

    if(app == NULL)
        return 0;

    png = flint_embedded_asset(LOCALE_FONT_PNG);
    dat = flint_embedded_asset(LOCALE_FONT_DAT);
    png_8 = flint_embedded_asset(LOCALE_FONT_8_PNG);
    dat_8 = flint_embedded_asset(LOCALE_FONT_8_DAT);
    if(png == NULL || dat == NULL || png_8 == NULL || dat_8 == NULL)
        return 0;

    font = flint_text_load_chopped_font_from_memory(png->data, png->size, dat->data, dat->size,
                                                    LOCALE_FONT_BASE_SIZE);
    if(font.texture.id == 0)
        return 0;
    font_8 = flint_text_load_chopped_font_from_memory(png_8->data, png_8->size,
                                                      dat_8->data, dat_8->size,
                                                      LOCALE_FONT_8_BASE_SIZE);
    if(font_8.texture.id == 0) {
        flint_text_unload_font(&font);
        return 0;
    }

    white = GenImageColor(1, 1, WHITE);
    app->font_shapes_texture = LoadTextureFromImage(white);
    UnloadImage(white);
    if(app->font_shapes_texture.id == 0) {
        flint_text_unload_font(&font);
        flint_text_unload_font(&font_8);
        return 0;
    }
    SetTextureFilter(app->font_shapes_texture, TEXTURE_FILTER_POINT);

    // Store the locale font in the app for use in text rendering
    app->locale_font = font;
    app->locale_font_8 = font_8;
    flint_text_set_font(font);
    flint_text_set_small_font(font_8);
    SetShapesTexture(app->font_shapes_texture, (Rectangle){0, 0, 1, 1});
    return 1;
}

static void
unload_locale_font(InbeApp *app)
{
    if(app == NULL)
        return;

    flint_text_set_font((Font){0});
    flint_text_set_small_font((Font){0});
    flint_text_unload_font(&app->locale_font);
    flint_text_unload_font(&app->locale_font_8);
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
    manual_screen_reset_layouts(app);
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
            Module.__inbeStorageSyncPending = true;
            Module.inbeSyncfsTimer = setTimeout(function() {
                Module.inbeSyncfsTimer = 0;
                Module.__inbeStorageSyncing = true;
                try {
                    FS.syncfs(false, function(err) {
                        Module.__inbeStorageSyncing = false;
                        Module.__inbeStorageSyncPending = false;
                        if(err) console.error("IDBFS save failed:", err);
                        else console.log("IDBFS synced");
                    });
                } catch(e) {
                    Module.__inbeStorageSyncing = false;
                    Module.__inbeStorageSyncPending = false;
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

    refresh_theme_colors(FLINT_THEME_SKY, 0);  /* Default: Sky light mode */

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

    app->tutorial_seen = inbe_storage_get_setting_int("tutorial_seen", 0) != 0;
    manual_seen_mask = inbe_storage_get_setting_int("exercise_manual_seen_mask", -1);
    if(manual_seen_mask < 0)
        manual_seen_mask = app->tutorial_seen ? exercise_manual_seen_bit(EXERCISE_WIM_HOF) : 0;
    app->exercise_manual_seen_mask = manual_seen_mask & ((1 << EXERCISE_COUNT) - 1);
    app->theme_id = clampi(inbe_storage_get_setting_int("theme", 0), 0, FLINT_THEME_COUNT - 1);
    app->theme_mode = clampi(inbe_storage_get_setting_int("theme_mode", APP_THEME_SYSTEM),
                             APP_THEME_SYSTEM, APP_THEME_DARK);
    app->dark_mode = inbe_storage_get_setting_int("dark_mode", 0) != 0;
    app->orientation_mode = clampi(inbe_storage_get_setting_int("orientation_mode", APP_ORIENTATION_SYSTEM),
                                   APP_ORIENTATION_SYSTEM, APP_ORIENTATION_SENSOR);
    app->main_tab = clampi(inbe_storage_get_setting_int("main_tab", APP_MAIN_TAB_PRACTICE),
                           APP_MAIN_TAB_HABITS, APP_MAIN_TAB_PRACTICE);
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
    app->practice_category_tab =
        clampi(inbe_storage_get_setting_int("practice_category_tab", PRACTICE_CATEGORY_MIND),
               0, PRACTICE_CATEGORY_COUNT - 1);
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
    app->locale_font_8 = (Font){0};

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
#ifdef __ANDROID__
    flint_ui_set_text_input_platform_callback(android_device_set_soft_keyboard_visible);
#endif

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
    app->habit_detail_index = -1;
    app->habit_detail_day = 0;
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
    app->settings_tab = SETTINGS_TAB_DEVICE;
    app->practice_config_tab = 0;
    app->practice_coming_soon_ticks = 0;
    app->habit_edit_active = 0;
    app->habit_edit_is_new = 0;
    app->habit_edit_index = -1;
    app->habit_edit_cursor = 0;
    app->habit_edit_focused = 0;
    app->habit_edit_text[0] = '\0';
    app->habit_edit_color = (Color){99, 196, 165, 255};
    app->habit_edit_sync_mode = INBE_HABIT_SYNC_NONE;
    app->habit_edit_sync_activity = 0;
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

    // Load all icons
    flint_load_all_icons(app->icons);

    /* Update tab bar icons */
    g_tabs[0].icon = app->icons[UI_ICON_TYPE_HABIT];
    g_tabs[0].icon_type = UI_ICON_TYPE_NONE;
    g_tabs[0].user_data = app;
    g_tabs[1].icon = app->icons[UI_ICON_TYPE_AMEN];
    g_tabs[1].icon_type = UI_ICON_TYPE_AMEN;
    g_tabs[1].user_data = app;
    g_tabs[1].on_click = on_practice_tab_click;
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

static void
meditation_finish(InbeApp *app)
{
    int duration;

    if(app == NULL)
        return;

    duration = app->meditation_duration_seconds;
    if(duration >= 60) {
        int session_duration = duration;
        if(data_save_session_path_for_activity(&session_duration, 1, 0, EXERCISE_MEDITATION,
                                               NULL, 0)) {
            sync_habits_for_activity(app, EXERCISE_MEDITATION);
        }
    }
    app_play_sound(app, app->bell_sound, 1.0f);
    app->meditation_duration_seconds = 0;
    app->meditation_remaining_seconds = 0;
    app->meditation_frame_ticks = 0;
    app->session_paused = 0;
    app->volume_popup_active = 0;
    meditation_music_stop(app);
    app->inbe.screen = InbeScreenStart;
}

static int
meditation_elapsed_seconds(InbeApp *app)
{
    int elapsed;

    if(app == NULL)
        return 0;
    elapsed = app->meditation_duration_seconds - app->meditation_remaining_seconds;
    if(elapsed < 0)
        elapsed = 0;
    if(elapsed > app->meditation_duration_seconds)
        elapsed = app->meditation_duration_seconds;
    return elapsed;
}

static int
meditation_save_elapsed(InbeApp *app)
{
    int elapsed;

    if(app == NULL)
        return 0;
    elapsed = meditation_elapsed_seconds(app);
    if(elapsed < 60)
        return 0;
    if(data_save_session_path_for_activity(&elapsed, 1, 0, EXERCISE_MEDITATION, NULL, 0)) {
        sync_habits_for_activity(app, EXERCISE_MEDITATION);
        return 1;
    }
    return 0;
}

static void
meditation_exit_to_start(InbeApp *app)
{
    if(app == NULL)
        return;
    meditation_music_stop(app);
    app->meditation_duration_seconds = 0;
    app->meditation_remaining_seconds = 0;
    app->meditation_frame_ticks = 0;
    app->session_paused = 0;
    app->volume_popup_active = 0;
    app->modal.active = 0;
    app->modal.type = UIModalNone;
    app->inbe.screen = InbeScreenStart;
}

static void
meditation_request_exit(InbeApp *app)
{
    if(app == NULL)
        return;
    app->modal.active = 1;
    app->modal.type = UIModalConfirmExitSession;
    app->modal.selected_button = 0;
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
    DrawRectangle(modal_x, modal_y, modal_w, modal_h, theme_get_surface());
    ui_draw_bevel(modal_x, modal_y, modal_w, modal_h,
                  flint_lighten(theme_get_surface(), 40), flint_darken(theme_get_surface(), 40));

    title_w = flint_text_measure(locale_get("meditation_title"), title_font);
    flint_text_draw(locale_get("meditation_title"), modal_x + (modal_w - title_w) / 2,
                    modal_y + flint_px(16), title_font, theme_get_text());

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

        DrawRectangle(popup_x, popup_y, popup_w, popup_h, theme_get_surface());
        ui_draw_bevel(popup_x, popup_y, popup_w, popup_h,
                      flint_lighten(theme_get_surface(), 40), flint_darken(theme_get_surface(), 40));

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
        meditation_request_exit(app);
        return;
    }

    draw_meditation_sound_controls(app);

    if(app->modal.active && app->modal.type == UIModalConfirmExitSession) {
        int elapsed = meditation_elapsed_seconds(app);
        int modal_result;

        if(elapsed >= 60) {
            modal_result = ui_draw_modal_3btn(locale_get("exit_session_title"),
                                              locale_get("meditation_save_elapsed_message"),
                                              locale_get("cancel_button"),
                                              locale_get("save_button"),
                                              locale_get("discard_button"));
            if(modal_result == 1) {
                app->modal.active = 0;
                app->modal.type = UIModalNone;
            } else if(modal_result == 2) {
                meditation_save_elapsed(app);
                meditation_exit_to_start(app);
            } else if(modal_result == 3) {
                meditation_exit_to_start(app);
            }
        } else {
            modal_result = ui_draw_modal(locale_get("exit_session_title"),
                                         locale_get("meditation_under_minute_exit_message"),
                                         locale_get("cancel_button"),
                                         locale_get("exit_button"));
            if(modal_result == 1) {
                app->modal.active = 0;
                app->modal.type = UIModalNone;
            } else if(modal_result == 2) {
                meditation_exit_to_start(app);
            }
        }
        return;
    }

    format_meditation_time(time_text, sizeof(time_text), app->meditation_remaining_seconds);
    max_w = view_width - flint_px(48);
    while(font > flint_px(28) && flint_text_measure(time_text, font) > max_w)
        font--;
    text_w = flint_text_measure(time_text, font);
    flint_text_draw(time_text, center_x - text_w / 2,
                    flint_ui_text_y(time_text, center_y - font, font * 2, font),
                    font, theme_get_text());

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
        settings_screen_clear_status();
        app->inbe.screen = app->main_tab == APP_MAIN_TAB_HABITS
                               ? InbeScreenHabits
                               : InbeScreenStart;
        app->settings_scroll = 0;
        break;

    case InbeScreenPracticeConfig:
        app_leave_practice_config(app);
        app->inbe.screen = InbeScreenStart;
        break;

    case InbeScreenHabits:
        app->inbe.screen = InbeScreenStart;
        break;

    case InbeScreenHabitEdit:
        habit_edit_cancel(app);
        app->inbe.screen = InbeScreenHabits;
        break;

    case InbeScreenHabitSessionEdit:
        habit_session_cancel_edit(app);
        app->inbe.screen = InbeScreenHabits;
        break;

    case InbeScreenLanguage:
        break;

    case InbeScreenManual:
        manual_screen_close_tutorial(app, 0);
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
        meditation_request_exit(app);
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

    app_apply_pending_bottom_tab(app);
    meditation_music_update(app);
    if(app->practice_coming_soon_ticks > 0)
        app->practice_coming_soon_ticks--;

    if(IsKeyPressed(KEY_BACK) ||
       (IsKeyPressed(KEY_BACKSPACE) &&
        !(app->inbe.screen == InbeScreenHabitEdit && app->habit_edit_active) &&
        !(app->inbe.screen == InbeScreenHabitSessionEdit && app->habit_session_edit_active))) {
        if(app->modal.active) {
            app->modal.active = 0;
            app->modal.type = UIModalNone;
        } else {
            handle_back_button(app);
        }
    }

    if(app->inbe.screen == InbeScreenSettings) {
        settings_screen_draw(app);
        app_draw_bottom_nav(app);
        goto finish_frame;
    }

    if(app->inbe.screen == InbeScreenPracticeConfig) {
        practice_config_screen_draw(app);
        app_draw_bottom_nav(app);
        goto finish_frame;
    }

    if(app->inbe.screen == InbeScreenLanguage) {
        language_screen_draw(app);
        goto finish_frame;
    }

    if(app->inbe.screen == InbeScreenManual) {
        manual_screen_draw(app);
        goto finish_frame;
    }

    if(app->inbe.screen == InbeScreenHabits) {
        draw_habits_screen(app);
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

    if(app->inbe.screen == InbeScreenStart) {
        session_update_circle_bounds_for_view(&app->inbe, flint_px(48),
                                      flint_px(58) + flint_px(64));
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
        DrawRectangle(0, 0, view_width, flint_px(58), flint_darken(theme_get_bg(), 14));
        DrawLine(0, flint_px(58) - 1, view_width, flint_px(58) - 1,
                 flint_darken(theme_get_bg(), 42));

        {
            const char *exercise_options[EXERCISE_COUNT];
            int exercise_values[EXERCISE_COUNT];
            int activity_count;
            int activity_index;
            int dropdown_w = flint_px(230);
            int dropdown_h = flint_px(36);
            int manual_icon_size = flint_px(20);
            int manual_icon_padding = flint_px(8);
            int manual_btn_w = manual_icon_size + manual_icon_padding * 2;
            int settings_icon_size = flint_px(20);
            int settings_icon_padding = flint_px(8);
            int settings_btn_w = settings_icon_size + settings_icon_padding * 2;
            int selector_gap = flint_px(8);
            int dropdown_x;
            int dropdown_y;
            int manual_x;
            int settings_x;
            int exercise_changed = 0;

            activity_count = EXERCISE_COUNT;
            for(int i = 0; i < activity_count; i++) {
                exercise_values[i] = i;
                exercise_options[i] = practice_activity_label(exercise_values[i]);
            }
            activity_index = clampi(app->exercise_type, 0, EXERCISE_COUNT - 1);

            settings_x = view_width - settings_btn_w - flint_px(10);
            manual_x = settings_x - selector_gap - manual_btn_w;
            dropdown_x = flint_px(12);
            dropdown_y = (flint_px(58) - dropdown_h) / 2;
            if(dropdown_x + dropdown_w > manual_x - selector_gap)
                dropdown_w = manual_x - selector_gap - dropdown_x;
            if(dropdown_w < flint_px(160))
                dropdown_w = flint_px(160);

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
                                       app->icons[UI_ICON_TYPE_WRENCH], &hover)) {
                reset_settings_preview(app);
                app->settings_scroll = 0;
                app->practice_config_tab = 0;
                app->inbe.screen = InbeScreenPracticeConfig;
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

    DrawRectangleRec(viewport, theme_get_bg());
    flint_clip_begin((int)viewport.x, (int)viewport.y, (int)viewport.width, (int)viewport.height);
        BeginMode2D(app->camera);
            DrawRectangle(0, 0, view_width, view_height, theme_get_bg());
            updateapp(app);
        EndMode2D();
    flint_clip_end();
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

void
inbe_app_destroy(void *vapp)
{
    InbeApp *app = vapp;
    if (app == NULL) return;

    if(app->settings_dirty || app->settings_save_delay_ticks > 0)
        save_settings(app);

    // Unload all icons
    flint_unload_all_icons(app->icons);
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
