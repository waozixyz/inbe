#include "app.h"
#include "data.h"
#include "locale.h"
#include "tabs/language_tab.h"
#include "tabs/manual_tab.h"
#include "tabs/settings_tab.h"
#include "screens/practice_screen.h"
#include "app_session.h"
#include "app_preferences.h"
#include "meditation_music.h"
#include "storage.h"
#include "theme.h"
#if defined(LOTUS_BUILD)
#include "lotus_settings.h"
#endif
#include "version.h"
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

static void habit_session_cancel_edit(InbeApp *app);
#define LOCALE_FONT_DAT "assets/fonts/locales.dat"
#define LOCALE_FONT_BASE_SIZE 16
static void habit_edit_delete_before_cursor(InbeApp *app);
static void habit_edit_delete_at_cursor(InbeApp *app);
static void habit_edit_insert_char(InbeApp *app, char ch);


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
    case InbeScreenHabitEdit:
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

static void on_settings_tab_click(void *user_data) {
    InbeApp *app = user_data;
    app_request_bottom_tab(app, APP_BOTTOM_TAB_SETTINGS);
}

static UITab g_tabs[] = {
    {NULL, {0}, UI_ICON_TYPE_HOME, on_habits_tab_click, NULL},
    {NULL, {0}, UI_ICON_TYPE_AMEN, NULL, NULL},
    {NULL, {0}, UI_ICON_TYPE_GEAR, on_settings_tab_click, NULL}
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
    app->settings_category = -1;
    app->settings_sub_tab = 0;
    app->app_settings_tab = APP_SETTINGS_TAB_APP;
    app->settings_from_exercise_selector = 0;
    app->practice_coming_soon_ticks = 0;
    app->habit_edit_active = 0;
    app->habit_edit_is_new = 0;
    app->habit_edit_index = -1;
    app->habit_edit_cursor = 0;
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

    case InbeScreenHabitEdit:
        habit_edit_commit(app);
        break;

    case InbeScreenHabitSessionEdit:
        habit_session_cancel_edit(app);
        app->inbe.screen = InbeScreenHabits;
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
draw_habit_edit_field(InbeApp *app, int x, int y, int w, int h, int font)
{
    FlintUITextInputStyle style = {
        .background = flint_darken(theme_get_bg(), 4),
        .border = theme_get_button(),
        .focus_border = theme_get_button_hover(),
        .text = theme_get_text(),
        .cursor = theme_get_text(),
        .radius = 0.08f,
        .padding_x = flint_px(10)
    };

    flint_ui_draw_text_input((Rectangle){(float)x, (float)y, (float)w, (float)h},
                             app != NULL ? app->habit_edit_text : "",
                             app != NULL ? app->habit_edit_cursor : 0,
                             1, ((app != NULL ? app->inbe.frame : 0) / 24) % 2,
                             font, style);
}

static int
habit_date_index(int year, int month, int day)
{
    return year * 10000 + month * 100 + day;
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
    if(ctx->sync_mode == INBE_HABIT_SYNC_ACTIVITIES &&
       (ctx->sync_activity & habit_activity_mask_for(activity)) == 0)
        return;
    (void)topic;
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
habit_open_linked_edit_page(InbeApp *app, int habit_index, int day_index)
{
    if(app == NULL)
        return;
    app->habit_detail_index = habit_index;
    app->habit_detail_day = day_index;
    app->habit_detail_session_path[0] = '\0';
    app->habit_session_edit_active = 0;
    app->habit_session_edit_kind = HABIT_SESSION_EDIT_NONE;
    app->habit_session_edit_round = -1;
    app->habit_session_edit_cursor = 0;
    app->habit_session_edit_path[0] = '\0';
    app->habit_session_edit_text[0] = '\0';
    app->habit_session_edit_scroll = 0;
    app->modal.active = 0;
    app->modal.type = 0;
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

static void
habit_edit_insert_char(InbeApp *app, char ch)
{
    int len;
    int cursor;

    if(app == NULL)
        return;
    habit_edit_clamp_cursor(app);
    len = (int)strlen(app->habit_edit_text);
    cursor = app->habit_edit_cursor;

    if(len < INBE_HABIT_NAME_SIZE - 1) {
        memmove(app->habit_edit_text + cursor + 1,
                app->habit_edit_text + cursor,
                (size_t)(len - cursor) + 1);
        app->habit_edit_text[cursor] = ch;
        app->habit_edit_cursor = cursor + 1;
    }
}

static void
draw_habits_top_bar(InbeApp *app, int draw_menu)
{
    static const char *options[INBE_HABIT_MAX + 1];
    static int dropdown_selected = 0;
    int option_count;
    int selected;
    int top_h = flint_px(58);
    int icon_size = flint_px(20);
    int icon_padding = flint_px(8);
    int icon_w = icon_size + icon_padding * 2;
    int right_x = view_width - icon_w - flint_px(10);
    int icon_y = (top_h - icon_w) / 2;
    int dropdown_x = flint_px(12);
    int dropdown_y = (top_h - flint_px(36)) / 2;
    int dropdown_w = right_x - dropdown_x - flint_px(10);
    int hover = 0;

    if(app == NULL)
        return;

    if(dropdown_w < flint_px(150))
        dropdown_w = flint_px(150);
    if(dropdown_w > flint_px(260))
        dropdown_w = flint_px(260);

    option_count = app->habits.count;
    if(option_count > INBE_HABIT_MAX)
        option_count = INBE_HABIT_MAX;
    for(int i = 0; i < option_count; i++)
        options[i] = app->habits.items[i].name;
    options[option_count++] = "Add new habit";

    selected = app->habits.selected;
    if(selected < 0 || selected >= app->habits.count)
        selected = 0;

    if(!draw_menu) {
        dropdown_selected = selected;

        DrawRectangle(0, 0, view_width, top_h, flint_darken(theme_get_bg(), 14));
        DrawLine(0, top_h - 1, view_width, top_h - 1, flint_darken(theme_get_bg(), 42));

        if(!app->modal.active &&
           ui_draw_dropdown_button(301, dropdown_x, dropdown_y, dropdown_w,
                                   flint_px(36), options, option_count, &dropdown_selected)) {
            if(dropdown_selected == app->habits.count) {
                habit_edit_begin_new(app);
                return;
            }
            app->habits.selected = dropdown_selected;
            inbe_habits_save(&app->habits);
        }

        if(!app->modal.active && app->habits.count > 0 &&
           ui_draw_icon_btn_padded(right_x, icon_y, icon_size, icon_padding,
                                   app->icons[UI_ICON_TYPE_PENCIL], &hover)) {
            habit_edit_begin(app, app->habits.selected);
            return;
        }

        return;
    }

    if(draw_menu && !app->modal.active && ui_draw_dropdown_menu(301)) {
        if(dropdown_selected == app->habits.count) {
            habit_edit_begin_new(app);
            return;
        }
        app->habits.selected = dropdown_selected;
        inbe_habits_save(&app->habits);
    }
}

static void
draw_habits_screen(InbeApp *app)
{
    int top_h = flint_px(58);
    int nav_h = flint_px(TAB_BAR_H);
    int content_x;
    int content_w;
    int y = top_h + flint_px(16);
    int viewport_h = view_height - top_h - nav_h;
    int hover = 0;
    int side_padding = flint_page_side_padding();
    int max_w = flint_px(CONTENT_MAX_W);
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

    if(app == NULL)
        return;

    draw_habits_top_bar(app, 0);
    if(app->inbe.screen != InbeScreenHabits)
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
                        empty_y, empty_font, theme_get_text());
        if(ui_draw_generic_button(content_x + (content_w - button_w) / 2,
                                  empty_y + flint_px(38), button_w, button_h,
                                  create_text, UI_BUTTON_STYLE_PRIMARY,
                                  0, &hover_empty_create)) {
            habit_edit_begin_new(app);
        }
        ui_end_scissor();
        draw_habits_top_bar(app, 1);
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
                    flint_px(22), theme_get_text());

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
                DrawRectangle(cell_x, cell_y, cell_w, cell_h, flint_darken(theme_get_bg(), 5));
                continue;
            }

            snprintf(day_label, sizeof(day_label), "%d", day);
            day_index = year * 10000 + mon * 100 + day;
            future_day = day_index > today_index;
            completed = inbe_habit_completed_day(active, day_index);
            if(!completed && !future_day && linked_ctx != NULL && habit_linked_has_day(linked_ctx, day_index))
                completed = 1;
            if(ui_draw_generic_button(cell_x, cell_y, cell_w, cell_h, day_label,
                                             completed ? UI_BUTTON_STYLE_PRIMARY : UI_BUTTON_STYLE_SECONDARY,
                                             future_day, &hover)) {
                if(active_is_linked || (linked_ctx != NULL && habit_linked_has_day(linked_ctx, day_index))) {
                    habit_open_linked_edit_page(app, selected, day_index);
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
                                     (float)flint_px(2), theme_get_text());
            }
        }
    }

    ui_end_scissor();
    free(linked_ctx);
    ui_set_input_blocked(0);
    draw_habits_top_bar(app, 1);
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
                    selected ? theme_get_text() : flint_darken(theme_get_bg(), 42));
    if(hovered) {
        app->cursor_clickable = 1;
        DrawCircleLines(x, y, radius + flint_px(5), theme_get_button_hover());
        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
            return 1;
    }
    return 0;
}

static void
draw_habit_edit_screen(InbeApp *app)
{
    const char *title;
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

    if(app == NULL)
        return;

    if(!app->habit_edit_active) {
        app->inbe.screen = InbeScreenHabits;
        return;
    }

    title = app->habit_edit_is_new ? "New Habit" : "Edit Habit";

    DrawRectangle(0, 0, view_width, top_h, theme_get_bg());
    DrawLine(0, top_h - 1, view_width, top_h - 1, flint_darken(theme_get_button(), 18));
    if(ui_draw_icon_btn_padded(flint_px(12), flint_px(12), flint_px(24),
                               flint_px(8), app->icons[UI_ICON_TYPE_RETURN], &hover)) {
        habit_edit_commit(app);
        return;
    }
    title_w = flint_text_measure(title, title_font);
    flint_text_draw(title, (view_width - title_w) / 2,
                    flint_ui_text_y(title, 0, top_h, title_font),
                    title_font, theme_get_text());
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

    flint_text_draw("Name", content_x, y, label_font, flint_darken(theme_get_text(), 34));
    y += flint_px(22);
    draw_habit_edit_field(app, content_x, y, content_w, field_h, font);
    habit_edit_update_input(app, content_x, y, content_w, field_h,
                            content_x + flint_px(10), font);
    y += field_h + flint_px(24);

    flint_text_draw("Underline", content_x, y, label_font, flint_darken(theme_get_text(), 34));
    y += flint_px(32);
    color_options[0] = (Color){94, 166, 232, 255};
    color_options[1] = (Color){99, 196, 165, 255};
    color_options[2] = (Color){210, 180, 72, 255};
    color_options[3] = (Color){224, 124, 104, 255};
    color_options[4] = (Color){180, 132, 220, 255};
    color_options[5] = (Color){216, 116, 164, 255};
    for(int i = 0; i < 6; i++) {
        int cx = content_x + flint_px(18) + i * flint_px(42);
        int selected = app->habit_edit_color.r == color_options[i].r &&
                       app->habit_edit_color.g == color_options[i].g &&
                       app->habit_edit_color.b == color_options[i].b;
        if(habit_color_button(app, cx, y, color_options[i], selected))
            app->habit_edit_color = color_options[i];
    }
    y += flint_px(34);

    flint_text_draw("Practice list", content_x, y, label_font, flint_darken(theme_get_text(), 34));
    y += flint_px(24);
    for(int i = 0; i < EXERCISE_COUNT; i++) {
        int enabled = (app->habit_edit_sync_activity & habit_activity_mask_for(i)) != 0;
        if(ui_draw_checkbox_toggle(content_x, y, activity_options[i], &enabled)) {
            if(enabled)
                app->habit_edit_sync_activity |= habit_activity_mask_for(i);
            else
                app->habit_edit_sync_activity &= ~habit_activity_mask_for(i);
            app->habit_edit_sync_mode = app->habit_edit_sync_activity != 0
                                            ? INBE_HABIT_SYNC_ACTIVITIES
                                            : INBE_HABIT_SYNC_NONE;
        }
        y += flint_px(42);
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
    ui_begin_scissor((int)viewport.x, (int)viewport.y, (int)viewport.width, (int)viewport.height);
        BeginMode2D(app->camera);
            DrawRectangle(0, 0, view_width, view_height, theme_get_bg());
            updateapp(app);
        EndMode2D();
    ui_end_scissor();
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
