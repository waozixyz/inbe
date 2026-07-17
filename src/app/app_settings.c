#include "app_settings.h"

#include "app.h"
#include "app_nav.h"
#include "device_preferences.h"
#include "locale.h"
#include "practices/meditation/meditation_music.h"
#include "practices/sun_salutation/sun_salutation_practice.h"
#include "practices/whm/whm_session.h"
#include "storage.h"
#include "theme.h"
#include "ui_dpi.h"
#include "ui.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(PLATFORM_WEB)
#include <emscripten.h>
#endif

typedef struct IntSetting {
    const char *key;
    int value;
} IntSetting;

typedef struct LoadedIntSetting {
    int *dst;
    const char *key;
    int fallback;
} LoadedIntSetting;

typedef struct LoadedBoolSetting {
    int *dst;
    const char *key;
    int fallback;
} LoadedBoolSetting;

typedef struct LoadedClampedSetting {
    int *dst;
    const char *key;
    int fallback;
    int min_value;
    int max_value;
} LoadedClampedSetting;

#define APP_SETTINGS_CACHE_MAX 96

typedef struct SettingsCacheEntry {
    char key[64];
    char value[256];
} SettingsCacheEntry;

typedef struct SettingsLoadCache {
    SettingsCacheEntry entries[APP_SETTINGS_CACHE_MAX];
    int count;
} SettingsLoadCache;

static void
settings_cache_add(const char *key, const char *value, void *user)
{
    SettingsLoadCache *cache = user;

    if(cache == NULL || key == NULL || key[0] == '\0' ||
       cache->count >= APP_SETTINGS_CACHE_MAX)
        return;
    snprintf(cache->entries[cache->count].key,
             sizeof(cache->entries[cache->count].key), "%s", key);
    snprintf(cache->entries[cache->count].value,
             sizeof(cache->entries[cache->count].value), "%s",
             value != NULL ? value : "");
    cache->count++;
}

static const char *
settings_cache_get(const SettingsLoadCache *cache, const char *key)
{
    if(cache == NULL || key == NULL)
        return NULL;
    for(int i = 0; i < cache->count; i++) {
        if(strcmp(cache->entries[i].key, key) == 0)
            return cache->entries[i].value;
    }
    return NULL;
}

static int
settings_cache_get_int(const SettingsLoadCache *cache, const char *key,
                       int fallback)
{
    const char *text = settings_cache_get(cache, key);
    return text != NULL && text[0] != '\0' ? atoi(text) : fallback;
}

static int
exercise_manual_bit(int exercise_type)
{
    if(exercise_type < 0 || exercise_type >= EXERCISE_COUNT)
        return 0;
    return 1 << exercise_type;
}

#if defined(PLATFORM_WEB)
static void
sync_web_storage(void)
{
    EM_ASM({
        if(typeof Module.__inbeFlushStorageSync === 'function')
            Module.__inbeFlushStorageSync(true);
        else if(typeof Module.__inbeScheduleStorageSync === 'function')
            Module.__inbeScheduleStorageSync(0, true);
    });
}
#endif

static void
save_int_settings(const IntSetting *settings, size_t count)
{
    for(size_t i = 0; i < count; i++)
        storage_set_setting_int(settings[i].key, settings[i].value);
}

static void
load_int_settings(const LoadedIntSetting *settings, size_t count,
                  const SettingsLoadCache *cache)
{
    for(size_t i = 0; i < count; i++)
        *settings[i].dst = settings_cache_get_int(cache, settings[i].key,
                                                  settings[i].fallback);
}

static void
load_bool_settings(const LoadedBoolSetting *settings, size_t count,
                   const SettingsLoadCache *cache)
{
    for(size_t i = 0; i < count; i++)
        *settings[i].dst = settings_cache_get_int(cache, settings[i].key,
                                                  settings[i].fallback) != 0;
}

static void
load_clamped_settings(const LoadedClampedSetting *settings, size_t count,
                      const SettingsLoadCache *cache)
{
    for(size_t i = 0; i < count; i++) {
        int value = settings_cache_get_int(cache, settings[i].key,
                                           settings[i].fallback);
        *settings[i].dst = clampi(value, settings[i].min_value,
                                  settings[i].max_value);
    }
}

static int
load_navigation_mode(const SettingsLoadCache *cache)
{
#if ANDROID_BUILD
    int default_nav = NAV_MODE_DROPDOWN;
#else
    int default_nav = NAV_MODE_TABBAR;
#endif
    int value = settings_cache_get_int(cache, "navigation_mode", default_nav);
    if(value == NAV_MODE_DROPDOWN)
        return NAV_MODE_DROPDOWN;
    return NAV_MODE_TABBAR;
}

static UIIconType
default_profile_picture_icon(void)
{
    UIIconType fallback = GetUIProfilePictureIconType(0);
    return fallback != UI_ICON_TYPE_NONE ? fallback : UI_ICON_TYPE_PROFILE;
}

static int
is_profile_picture_icon(UIIconType type)
{
    for(int i = 0; i < GetUIProfilePictureIconCount(); i++) {
        if(GetUIProfilePictureIconType(i) == type)
            return 1;
    }
    return 0;
}

static UIIconType
load_profile_picture_icon(const SettingsLoadCache *cache)
{
    UIIconType fallback = default_profile_picture_icon();
    UIIconType type =
        (UIIconType)settings_cache_get_int(cache, "profile_picture_icon", fallback);

    return is_profile_picture_icon(type) ? type : fallback;
}

static void
load_bottom_nav_routes(InbeApp *app, const SettingsLoadCache *cache)
{
    if(app == NULL)
        return;
    app->bottom_nav_route_count =
        settings_cache_get_int(cache, "bottom_nav_route_count", 2);
    app->bottom_nav_routes[0] =
        settings_cache_get_int(cache, "bottom_nav_route_0", APP_NAV_ROUTE_HABITS);
    app->bottom_nav_routes[1] =
        settings_cache_get_int(cache, "bottom_nav_route_1", APP_NAV_ROUTE_PRACTICE);
    app->bottom_nav_routes[2] =
        settings_cache_get_int(cache, "bottom_nav_route_2", APP_NAV_ROUTE_NONE);
    app->bottom_nav_routes[3] =
        settings_cache_get_int(cache, "bottom_nav_route_3", APP_NAV_ROUTE_NONE);
    app_sanitize_bottom_nav_routes(app);
}

static int
default_theme_source(void)
{
#if defined(PLATFORM_DESKTOP)
    return APP_THEME_SOURCE_SYSTEM;
#else
    return APP_THEME_SOURCE_APP;
#endif
}

void
apply_settings(Inbe *inbe, int speed, int max_rounds, int max_breaths, int pause_seconds)
{
    speed = clampi(speed, SETTINGS_SPEED_MIN, SETTINGS_SPEED_MAX);
    inbe->speed_level = speed;
    inbe->breath_half_ticks = breath_half_ticks_for_speed(speed);
    inbe->breath_animation = clampi(inbe->breath_animation,
                                    InbeBreathAnimationLinear,
                                    InbeBreathAnimationCount - 1);
    inbe->progressive_start_speed = clampi(inbe->progressive_start_speed,
                                           SETTINGS_SPEED_MIN, speed);
    inbe->max_rounds = clampi(max_rounds, 1, MaxRounds);
    inbe->pause_seconds = clampi(pause_seconds, SETTINGS_PAUSE_MIN, SETTINGS_PAUSE_MAX);
    count_from_int(inbe->maxbreaths,
                   clampi(max_breaths, SETTINGS_BREATHS_MIN, SETTINGS_BREATHS_MAX));
}

void
reset_settings_preview(InbeApp *app)
{
    int speed = app->inbe.speed_level;
    int max_rounds = app->inbe.max_rounds;
    int max_breaths = int_from_count(app->inbe.maxbreaths);
    int pause_seconds = app->inbe.pause_seconds;
    int play_in_background = app->inbe.play_in_background;
    int breath_animation = app->inbe.breath_animation;
    int content_w;

    inbeinit(&app->settings_preview);
    app->settings_preview.progressive_speed = 0;
    app->settings_preview.play_in_background = play_in_background;
    app->settings_preview.breath_animation = breath_animation;
    GetUICenteredColumn(CONTENT_MAX_W, CONTENT_SIDE_PAD, NULL, &content_w);
    update_preview_bounds(&app->settings_preview, content_w, ScaleUIPx(132));
    apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
    session_reset_round_breathe(&app->settings_preview);
}

void
save_settings(InbeApp *app)
{
    if(app == NULL)
        return;

    const IntSetting int_settings[] = {
        {"speed", app->inbe.speed_level},
        {"max_rounds", app->inbe.max_rounds},
        {"max_breaths", int_from_count(app->inbe.maxbreaths)},
        {"pause_seconds", app->inbe.pause_seconds},
        {"sound_volume", app->sound_volume},
        {"tutorial_seen", app->tutorial_seen ? 1 : 0},
        {"habits_guide_seen", app->habits_guide_seen ? 1 : 0},
        {"exercise_manual_seen_mask", app->exercise_manual_seen_mask},
        {"practice_visible_mask", app->practice_visible_mask},
        {"theme", app->theme_id},
        {"theme_source", app->theme_source},
        {"dark_mode", app->dark_mode},
        {"theme_mode", app->theme_mode},
        {"orientation_mode", app->orientation_mode},
        {"navigation_mode", app->navigation_mode},
        {"profile_picture_icon", app->profile_picture_icon},
        {"bottom_nav_route_count", app->bottom_nav_route_count},
        {"bottom_nav_route_0", app->bottom_nav_routes[0]},
        {"bottom_nav_route_1", app->bottom_nav_routes[1]},
        {"bottom_nav_route_2", app->bottom_nav_routes[2]},
        {"bottom_nav_route_3", app->bottom_nav_routes[3]},
        {"transition_mode", app->transition_mode},
        {"main_tab", app->main_tab},
        {"habits_screen_mode", app->habits.screen_mode == HABITS_SCREEN_REORDER
                                   ? HABITS_SCREEN_OVERVIEW
                                   : app->habits.screen_mode},
        {"habits_tab", app->habits.tab},
        {"habits_view_mode", app->habits.view_mode},
        {"fullscreen", app->fullscreen_enabled ? 1 : 0},
        {"on_screen_keyboard", app->on_screen_keyboard_enabled ? 1 : 0},
        {"progressive_speed", app->inbe.progressive_speed},
        {"progressive_start_speed", app->inbe.progressive_start_speed},
        {"breath_animation", app->inbe.breath_animation},
        {"advanced_session_controls", app->advanced_session_controls ? 1 : 0},
        {"double_tap_to_breathe", app->double_tap_to_breathe ? 1 : 0},
        {"show_session_return_button", app->show_session_return_button ? 1 : 0},
        {"hold_display_mode", app->hold_display_mode},
        {"exercise_type", app->exercise_type},
        {"sun_salutation_repetitions", app->sun_salutation.repetitions},
        {"sun_salutation_start_seconds", app->sun_salutation.start_seconds},
        {"sun_salutation_end_seconds", app->sun_salutation.end_seconds},
        {"sun_salutation_figure", app->sun_salutation.figure},
        {"meditation_duration_mode", app->meditation.duration_mode},
        {"meditation_custom_minutes", app->meditation.custom_minutes},
        {"meditation_show_extend_controls", app->meditation.show_extend_controls ? 1 : 0},
        {"meditation_music_track", app->meditation.music_track},
        {"practice_music_mask", app->meditation.music_practice_mask},
        {"practice_music_track_wim_hof", app->meditation.music_practice_tracks[EXERCISE_WIM_HOF]},
        {"practice_music_track_meditation", app->meditation.music_practice_tracks[EXERCISE_MEDITATION]},
        {"practice_music_track_sun_salutation", app->meditation.music_practice_tracks[EXERCISE_SUN_SALUTATION]},
        {"play_in_background", app->inbe.play_in_background},
        {"practice_category_tab", app->practice_category_tab},
    };

    storage_settings_begin_write();
    save_int_settings(int_settings, sizeof(int_settings) / sizeof(int_settings[0]));
    storage_set_setting_text("language",
                                  (app->language_selected && app->language[0] != '\0') ?
                                      app->language : "");
    storage_set_setting_text("habits_selected_id",
                             (app->habits.selected >= 0 &&
                              app->habits.selected < app->habits.count)
                                 ? app->habits.items[app->habits.selected].id
                                 : "");
    storage_settings_end_write();
#if defined(PLATFORM_WEB)
    sync_web_storage();
#endif
    app->settings_dirty = 0;
    app->settings_save_delay_ticks = 0;
}

static void
load_language_setting(InbeApp *app, int settings_missing,
                      const SettingsLoadCache *cache)
{
    const char *language = settings_cache_get(cache, "language");

    app->language_needs_save = 0;
    if(language != NULL && language[0] != '\0') {
        snprintf(app->language, sizeof(app->language), "%s", language);
        app->language_selected = 1;
        if(!SetLocale(app->language)) {
            snprintf(app->language, sizeof(app->language), "%s", "en");
            SetLocale(app->language);
        }
    } else {
        snprintf(app->language, sizeof(app->language), "%s", "en");
        app->language_selected = settings_missing ? 0 : 1;
        app->language_needs_save = app->language_selected;
        SetLocale(app->language);
    }

    app->language_index = GetCurrentLocaleIndex();
    if(app->language_index < 0)
        app->language_index = 0;
}

int
app_load_settings(InbeApp *app)
{
    int settings_missing;
    int speed;
    int max_rounds;
    int max_breaths;
    int pause_seconds;
    int manual_seen_mask;
    SettingsLoadCache settings_cache = {0};

    if(app == NULL)
        return 0;

    settings_missing = storage_settings_empty();
    storage_list_settings(settings_cache_add, &settings_cache);

    {
        LoadedIntSetting settings[] = {
            {&speed, "speed", DefaultSpeedLevel},
            {&max_rounds, "max_rounds", DefaultMaxRounds},
            {&max_breaths, "max_breaths", DefaultMaxBreaths},
            {&pause_seconds, "pause_seconds", DefaultPauseSeconds},
        };
        load_int_settings(settings, sizeof(settings) / sizeof(settings[0]),
                          &settings_cache);
    }

    {
        LoadedBoolSetting settings[] = {
            {&app->tutorial_seen, "tutorial_seen", 0},
            {&app->habits_guide_seen, "habits_guide_seen", 0},
            {&app->dark_mode, "dark_mode", 0},
            {&app->fullscreen_enabled, "fullscreen", 0},
#if ANDROID_BUILD || defined(PLATFORM_WEB)
            {&app->on_screen_keyboard_enabled, "on_screen_keyboard", 1},
#else
            {&app->on_screen_keyboard_enabled, "on_screen_keyboard", 0},
#endif
            {&app->inbe.progressive_speed, "progressive_speed", 1},
            {&app->advanced_session_controls, "advanced_session_controls", 0},
            {&app->double_tap_to_breathe, "double_tap_to_breathe", 0},
            {&app->show_session_return_button, "show_session_return_button", 1},
            {&app->meditation.show_extend_controls, "meditation_show_extend_controls", 1},
        };
        load_bool_settings(settings, sizeof(settings) / sizeof(settings[0]),
                           &settings_cache);
    }
#if ANDROID_BUILD || defined(PLATFORM_WEB)
    app->on_screen_keyboard_enabled = 1;
#endif

    {
        LoadedClampedSetting settings[] = {
            {&app->sound_volume, "sound_volume", 100,
             SETTINGS_VOLUME_MIN, SETTINGS_VOLUME_MAX},
            {&app->theme_id, "theme", 0, 0, THEME_COUNT - 1},
            {&app->theme_source, "theme_source", default_theme_source(),
             APP_THEME_SOURCE_APP, APP_THEME_SOURCE_SYSTEM},
            {&app->theme_mode, "theme_mode", APP_THEME_SYSTEM,
             APP_THEME_SYSTEM, APP_THEME_DARK},
            {&app->orientation_mode, "orientation_mode", APP_ORIENTATION_SYSTEM,
             APP_ORIENTATION_SYSTEM, APP_ORIENTATION_SENSOR},
            {&app->main_tab, "main_tab", APP_MAIN_TAB_PRACTICE,
             APP_MAIN_TAB_NONE, APP_MAIN_TAB_PRACTICE},
            {&app->habits.screen_mode, "habits_screen_mode", HABITS_SCREEN_OVERVIEW,
             HABITS_SCREEN_OVERVIEW, HABITS_SCREEN_DETAIL},
            {&app->habits.tab, "habits_tab", HABIT_TAB_WEEKLY,
             HABIT_TAB_WEEKLY, HABIT_TAB_COUNT - 1},
            {&app->habits.view_mode, "habits_view_mode", HABIT_VIEW_WEEKLY,
             HABIT_VIEW_CALENDAR, HABIT_VIEW_WEEKLY},
            {&app->inbe.progressive_start_speed, "progressive_start_speed",
             DefaultProgressiveStartSpeed, SETTINGS_SPEED_MIN, SETTINGS_SPEED_MAX},
            {&app->inbe.breath_animation, "breath_animation", InbeBreathAnimationLinear,
             InbeBreathAnimationLinear, InbeBreathAnimationCount - 1},
            {&app->hold_display_mode, "hold_display_mode", HOLD_DISPLAY_CIRCLE,
             HOLD_DISPLAY_CIRCLE, HOLD_DISPLAY_STOPWATCH},
            {&app->exercise_type, "exercise_type", EXERCISE_WIM_HOF,
             EXERCISE_WIM_HOF, EXERCISE_COUNT - 1},
            {&app->sun_salutation.repetitions, "sun_salutation_repetitions", 3,
             2, 12},
            {&app->sun_salutation.start_seconds, "sun_salutation_start_seconds", 8,
             3, 12},
            {&app->sun_salutation.end_seconds, "sun_salutation_end_seconds", 5,
             3, 12},
            {&app->sun_salutation.figure, "sun_salutation_figure", 0,
             0, SUN_SALUTATION_ACTIVE_FIGURE_COUNT - 1},
            {&app->meditation.duration_mode, "meditation_duration_mode", 1,
             0, 5},
            {&app->meditation.custom_minutes, "meditation_custom_minutes", 20,
             1, 240},
            {&app->meditation.music_track, "meditation_music_track", 0,
             0, MEDITATION_MUSIC_TRACK_COUNT - 1},
            {&app->meditation.music_practice_mask, "practice_music_mask",
             1 << EXERCISE_MEDITATION, 0, (1 << EXERCISE_COUNT) - 1},
            {&app->meditation.music_practice_tracks[EXERCISE_WIM_HOF],
             "practice_music_track_wim_hof", 0, 0, MEDITATION_MUSIC_TRACK_COUNT - 1},
            {&app->meditation.music_practice_tracks[EXERCISE_MEDITATION],
             "practice_music_track_meditation", 0, 0, MEDITATION_MUSIC_TRACK_COUNT - 1},
            {&app->meditation.music_practice_tracks[EXERCISE_SUN_SALUTATION],
             "practice_music_track_sun_salutation", 0, 0, MEDITATION_MUSIC_TRACK_COUNT - 1},
            {&app->practice_category_tab, "practice_category_tab", PRACTICE_CATEGORY_MIND,
             0, PRACTICE_CATEGORY_COUNT - 1},
            {&app->transition_mode, "transition_mode", APP_TRANSITION_NONE,
             APP_TRANSITION_NONE, APP_TRANSITION_FADE},
        };
        load_clamped_settings(settings, sizeof(settings) / sizeof(settings[0]),
                              &settings_cache);
    }

    app->navigation_mode = load_navigation_mode(&settings_cache);
    app->profile_picture_icon = load_profile_picture_icon(&settings_cache);
    load_bottom_nav_routes(app, &settings_cache);
    if(app->bottom_nav_route_count <= 0) {
        app->main_tab = APP_MAIN_TAB_NONE;
    } else if(app->main_tab == APP_MAIN_TAB_NONE) {
        app->main_tab = app->bottom_nav_routes[0] == APP_NAV_ROUTE_HABITS
                            ? APP_MAIN_TAB_HABITS
                            : APP_MAIN_TAB_PRACTICE;
    }

    manual_seen_mask = settings_cache_get_int(&settings_cache,
                                           "exercise_manual_seen_mask", -1);
    if(manual_seen_mask < 0)
        manual_seen_mask = app->tutorial_seen ? exercise_manual_bit(EXERCISE_WIM_HOF) : 0;
    app->exercise_manual_seen_mask = manual_seen_mask & ((1 << EXERCISE_COUNT) - 1);
    app->practice_visible_mask = settings_cache_get_int(&settings_cache,
                                                         "practice_visible_mask",
                                                         (1 << EXERCISE_COUNT) - 1);
    app->practice_visible_mask &= (1 << EXERCISE_COUNT) - 1;
    if(app->practice_visible_mask == 0)
        app->practice_visible_mask = (1 << EXERCISE_COUNT) - 1;

    load_language_setting(app, settings_missing, &settings_cache);

    // Check for environment variable override for dark mode
    if(getenv("INBE_FORCE_DARK_MODE") != NULL) {
        app->theme_mode = APP_THEME_DARK;
    }
#if defined(PLATFORM_WEB)
    app->transition_mode = APP_TRANSITION_NONE;
#endif

    app->inbe.play_in_background =
        settings_cache_get_int(&settings_cache, "play_in_background", 1) != 0;
    TraceLog(LOG_INFO, "INBE: Loaded play_in_background setting = %d",
             app->inbe.play_in_background);
    app->backgrounded = 0;

    app_device_preferences_init(app);
    apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
    refresh_locale_dependent_text(app);

    return settings_missing;
}
