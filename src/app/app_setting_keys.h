#ifndef INBE_APP_SETTING_KEYS_H
#define INBE_APP_SETTING_KEYS_H

#include <string.h>

#define INBE_IMPORTABLE_SETTING_KEYS(X) \
    X("speed") \
    X("max_rounds") \
    X("max_breaths") \
    X("pause_seconds") \
    X("sound_volume") \
    X("music_volume") \
    X("audio_cue_breath_in") \
    X("audio_cue_breath_out") \
    X("audio_cue_bell") \
    X("audio_custom_sound_count") \
    X("audio_custom_music_count") \
    X("retention_marker_enabled") \
    X("tutorial_seen") \
    X("exercise_manual_seen_mask") \
    X("theme") \
    X("theme_source") \
    X("dark_mode") \
    X("theme_mode") \
    X("theme_style") \
    X("orientation_mode") \
    X("ui_scale") \
    X("main_tab") \
    X("fullscreen") \
    X("on_screen_keyboard") \
    X("progressive_speed") \
    X("progressive_start_speed") \
    X("breath_animation") \
    X("double_tap_to_breathe") \
    X("advanced_session_controls") \
    X("exercise_type") \
    X("sun_salutation_repetitions") \
    X("sun_salutation_start_seconds") \
    X("sun_salutation_end_seconds") \
    X("sun_salutation_figure") \
    X("meditation_duration_mode") \
    X("meditation_custom_minutes") \
    X("meditation_show_extend_controls") \
    X("meditation_interval_bell") \
    X("meditation_music_track") \
    X("practice_music_mask") \
    X("practice_music_track_wim_hof") \
    X("practice_music_track_meditation") \
    X("practice_music_track_sun_salutation") \
    X("play_in_background") \
    X("language") \
    X("language_setup_done") \
    X("practice_category_tab") \
    X("patterns_preset") \
    X("patterns_custom_in") \
    X("patterns_custom_hold_in") \
    X("patterns_custom_exhale") \
    X("patterns_custom_hold_out") \
    X("patterns_duration_minutes") \
    X("breaks_enabled") \
    X("desktop_startup_mode") \
    X("desktop_close_action") \
    X("break_block_mode") \
    X("break_sounds_enabled") \
    X("break_hud_x") \
    X("break_hud_y") \
    X("break_reading_mode") \
    X("break_micro_counts_as_activity") \
    X("break_micro_enabled") \
    X("break_micro_limit") \
    X("break_micro_duration") \
    X("break_micro_postpone") \
    X("break_micro_max_prompts") \
    X("break_micro_show_skip") \
    X("break_micro_show_postpone") \
    X("break_rest_enabled") \
    X("break_rest_limit") \
    X("break_rest_duration") \
    X("break_rest_postpone") \
    X("break_rest_max_prompts") \
    X("break_rest_show_skip") \
    X("break_rest_show_postpone") \
    X("break_daily_enabled") \
    X("break_daily_limit") \
    X("break_daily_postpone") \
    X("break_daily_max_prompts") \
    X("break_daily_show_skip") \
    X("break_daily_show_postpone") \
    X("break_daily_state_day") \
    X("break_daily_state_active") \
    X("break_daily_state_skipped")

static inline int
app_setting_key_has_custom_audio_suffix(const char *key, const char *prefix)
{
    const char *suffix;
    size_t prefix_len;

    if(key == NULL || prefix == NULL)
        return 0;
    prefix_len = strlen(prefix);
    if(strncmp(key, prefix, prefix_len) != 0)
        return 0;

    suffix = key + prefix_len;
    if(*suffix < '0' || *suffix > '9')
        return 0;
    while(*suffix >= '0' && *suffix <= '9')
        suffix++;
    return strcmp(suffix, "_title") == 0 || strcmp(suffix, "_path") == 0;
}

static inline int
app_setting_key_importable(const char *key)
{
#define INBE_SETTING_KEY_ENTRY(name) name,
    static const char *const keys[] = {
        INBE_IMPORTABLE_SETTING_KEYS(INBE_SETTING_KEY_ENTRY)
    };
#undef INBE_SETTING_KEY_ENTRY

    if(key == NULL || key[0] == '\0')
        return 0;
    if(app_setting_key_has_custom_audio_suffix(key, "audio_custom_sound_") ||
       app_setting_key_has_custom_audio_suffix(key, "audio_custom_music_"))
        return 1;
    for(size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        if(strcmp(key, keys[i]) == 0)
            return 1;
    }
    return 0;
}

#endif
