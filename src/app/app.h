#ifndef INBE_APP_H
#define INBE_APP_H

#include "raylib.h"
#include "breath_engine.h"
#include "app_fwd.h"
#include "flint_runtime_assets.h"
#include "ui_icon_types.h"
#include "screens/habits_screen.h"

enum {
    SETTINGS_SPEED_MIN = 1,
    SETTINGS_SPEED_MAX = 8,
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
    CIRCLE_SIDE_PAD = 32,
    FS_PATH_MAX = 512
};

enum {
    ICON_SIZE_SMALL = 22,
    ICON_SIZE_MEDIUM = 26,
    ICON_SIZE_LARGE = 30,
    ICON_SIZE_SMALL_MIN = 20,
    ICON_SIZE_SMALL_MAX = 36,
    ICON_SIZE_MEDIUM_MIN = 24,
    ICON_SIZE_MEDIUM_MAX = 40,
    ICON_SIZE_LARGE_MIN = 28,
    ICON_SIZE_LARGE_MAX = 44
};

enum {
    SETTINGS_TAB_DEVICE = 0,
    SETTINGS_TAB_THEME,
    SETTINGS_TAB_DATA,
    SETTINGS_TAB_COUNT
};

enum {
    PRACTICE_CATEGORY_MIND = 0,
    PRACTICE_CATEGORY_YOGA,
    PRACTICE_CATEGORY_FITNESS,
    PRACTICE_CATEGORY_COUNT
};

typedef enum {
    UIModalNone,
    UIModalConfirmExitSession,
    UIModalMeditationSetup,
    UIModalConfirmDeleteData,
    UIModalConfirmDeleteHabit,
    UIModalEditProgressiveStartSpeed,
    UIModalMeditationNetworkError,
    UIModalConfirmImportDataSettings,
    UIModalSyncAccountBackup,
} UIModalType;

typedef struct {
    int active;
    UIModalType type;
    int selected_button;
} UIModal;

typedef struct InbeConfig {
	char title[64];
	int width;
	int height;
	int loaded;
    int title_custom;
} InbeConfig;

extern InbeConfig config;

typedef enum HoldDisplayMode {
    HOLD_DISPLAY_CIRCLE = 0,
    HOLD_DISPLAY_STOPWATCH = 1,
} HoldDisplayMode;

typedef enum ExerciseType {
    EXERCISE_WIM_HOF = 0,
    EXERCISE_MEDITATION = 1,
    EXERCISE_COUNT = 2
} ExerciseType;

typedef enum AppThemeMode {
    APP_THEME_SYSTEM = 0,
    APP_THEME_LIGHT = 1,
    APP_THEME_DARK = 2,
} AppThemeMode;

typedef enum AppOrientationMode {
    APP_ORIENTATION_SYSTEM = 0,
    APP_ORIENTATION_PORTRAIT = 1,
    APP_ORIENTATION_LANDSCAPE = 2,
    APP_ORIENTATION_SENSOR = 3,
} AppOrientationMode;

typedef enum AppDeviceOrientation {
    APP_DEVICE_ORIENTATION_UNKNOWN = 0,
    APP_DEVICE_ORIENTATION_PORTRAIT = 1,
    APP_DEVICE_ORIENTATION_LANDSCAPE = 2,
} AppDeviceOrientation;

typedef enum AppMainTab {
    APP_MAIN_TAB_HABITS = 0,
    APP_MAIN_TAB_PRACTICE = 1,
} AppMainTab;

typedef struct WhmPracticeState {
    Texture2D image_1;
    Texture2D image_2;
} WhmPracticeState;

typedef struct MeditationPracticeState {
    Texture2D image_1;
    int duration_seconds;
    int remaining_seconds;
    int frame_ticks;
    int music_enabled;
    int music_shuffle;
    int music_track;
    int music_loaded;
    int music_playing;
    int music_test_playing;
    int music_archive_extracted;
    int music_network_error_notified;
    Music music;
    FlintRuntimeAssetDownload music_download;
    char music_cache_dir[FS_PATH_MAX];
    char music_status[128];
} MeditationPracticeState;

struct InbeApp {
    Inbe inbe;
    Inbe settings_preview;
    Inbe start_speed_preview;
    int start_speed_preview_speed;
    Camera2D camera;
    int cursor_clickable;
    int cursor_disabled;
    Texture2D icons[UI_ICON_TYPE_COUNT];

    WhmPracticeState whm;
    MeditationPracticeState meditation;
    Texture2D font_shapes_texture;
    Font locale_font;
    Font locale_font_8;
    Sound breath_in_sound;
    Sound breath_out_sound;
    Sound bell_sound;
    int audio_ready;
    int sound_volume;
    int sound_last_screen;
    int sound_last_phase;
    int sound_last_dir;
    char sound_last_count[CountSize];
    int settings_scroll;
    int settings_drag_slider;
    int settings_drag_scrollbar;
    int settings_drag_content;
    int settings_drag_content_y;
    int settings_dirty;
    int settings_save_delay_ticks;
    int settings_tab;
    int device_picker_open;
    int device_picker_scroll;
    int fullscreen_enabled;
    int on_screen_keyboard_enabled;
    char language[16];
    int language_selected;
    int language_index;
    int language_needs_save;
    int manual_scroll;
    int manual_drag_scrollbar;
    int manual_drag_content;
    int manual_drag_content_y;
    int tutorial_step;
    int tutorial_seen;
    int exercise_manual_seen_mask;

    int theme_id;
    int dark_mode;
    int theme_mode;
    int orientation_mode;
    int android_orientation;
    int main_tab;
    int pending_bottom_tab;
    InbeHabits habits;
    int habit_detail_index;
    int habit_detail_day;
    char habit_detail_session_path[FS_PATH_MAX];
    int habit_session_edit_scroll;
    int habit_session_edit_active;
    int habit_session_edit_kind;
    int habit_session_edit_round;
    int habit_session_edit_cursor;
    char habit_session_edit_path[FS_PATH_MAX];
    char habit_session_edit_text[16];
    int habit_edit_active;
    int habit_edit_is_new;
    int habit_edit_index;
    int habit_edit_cursor;
    int habit_edit_focused;
    char habit_edit_text[INBE_HABIT_NAME_SIZE];
    Color habit_edit_color;
    int habit_edit_sync_mode;
    int habit_edit_sync_activity;
    int advanced_session_controls;
    int hold_display_mode;
    int exercise_type;
    int practice_config_tab;
    int practice_category_tab;
    int practice_coming_soon_ticks;
    int previous_screen;
    int session_paused;
    int backgrounded;
    int results_saved;
    char results_path[FS_PATH_MAX];
    int saved_pause_seconds;
    int volume_popup_active;
    UIModal modal;
    int play_circle_hover;
    float play_circle_scale;
};

void inbe_app_init(void *app);
void inbe_app_update_draw(void *app, Rectangle viewport);
void inbe_app_destroy(void *app);
void app_play_sound(InbeApp *app, Sound sound, float scale);
Texture2D app_load_asset_texture(const char *name);
void app_unload_texture(Texture2D texture);

int clampi(int x, int min, int max);
int int_from_count(const char src[4]);
void count_from_int(char dst[4], int value);
void app_reload_after_import(InbeApp *app, int reload_settings);
void update_preview_bounds(Inbe *inbe, int content_w, int max_h);
void refresh_theme_colors(int theme_id, int dark_mode);
void refresh_locale_dependent_text(InbeApp *app);
void apply_language_selection(InbeApp *app, int language_index, int save_now);
int exercise_manual_seen(InbeApp *app, int exercise_type);
void mark_exercise_manual_seen(InbeApp *app, int exercise_type);
void sync_habits_for_activity(InbeApp *app, int exercise_type);
void draw_preview_inbe(Inbe *inbe, int center_x, int center_y);

#include "app_settings.h"

#endif
