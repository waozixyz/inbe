#ifndef INBE_APP_H
#define INBE_APP_H

#include "kryon.h"
#include "platform.h"
#include "core/breath_engine.h"
#include "app_fwd.h"
#include "runtime_assets.h"
#include "screens/habits_screen.h"
#include "screens/settings/settings_types.h"
#include "storage/sync_account.h"

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
    SETTINGS_TAB_AUDIO,
    SETTINGS_TAB_THEME,
    SETTINGS_TAB_ABOUT,
    SETTINGS_TAB_COUNT
};

enum {
    INBE_AUDIO_CUE_BREATH_IN = 0,
    INBE_AUDIO_CUE_BREATH_OUT,
    INBE_AUDIO_CUE_BELL,
    INBE_AUDIO_CUE_COUNT
};

enum {
    INBE_AUDIO_BUILTIN_MUSIC_COUNT = 3,
    INBE_AUDIO_CUSTOM_SOUND_MAX = 8,
    INBE_AUDIO_CUSTOM_MUSIC_MAX = 16,
    INBE_AUDIO_LABEL_SIZE = 64,
    INBE_AUDIO_MUSIC_COUNT_MAX =
        INBE_AUDIO_BUILTIN_MUSIC_COUNT + INBE_AUDIO_CUSTOM_MUSIC_MAX,
    /* Sentinel for "no music selected" on a per-practice track slot. A value
     * of -1 means the practice plays no background music. */
    INBE_AUDIO_MUSIC_NONE = -1
};

enum {
    PROFILE_VIEW_MAIN = 0,
    PROFILE_VIEW_DATA,
    PROFILE_VIEW_SYNC_ACCOUNT,
    PROFILE_VIEW_HABITS,
    PROFILE_VIEW_PRACTICES,
};

enum {
    PROFILE_TAB_SYNC = 0,
    PROFILE_TAB_OVERVIEW = PROFILE_TAB_SYNC,
    PROFILE_TAB_DATA,
    PROFILE_TAB_FRIENDS,
    PROFILE_TAB_LEADERBOARD,
    PROFILE_TAB_COUNT
};

enum {
    PRACTICE_CATEGORY_MIND = 0,
    PRACTICE_CATEGORY_YOGA,
    PRACTICE_CATEGORY_FITNESS,
    PRACTICE_CATEGORY_COUNT
};

enum {
    PRACTICE_TAB_MANUAL = 0,
    PRACTICE_TAB_PLAY,
    PRACTICE_TAB_CONFIG,
    PRACTICE_TAB_COUNT
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
    UIModalConfirmDeleteSyncAccount,
    UIModalHabitPracticeListInfo,
    UIModalHabitCountingInfo,
    UIModalPracticeManual,
    UIModalPracticeConfig,
    UIModalThemePicker,
    UIModalSyncReview,
    UIModalSyncAlias,
    UIModalSyncPublicId,
    UIModalConfirmRemoveFriend,
    UIModalConfirmSyncAccountSwitch,
    UIModalBottomNavConfig,
    UIModalProfilePicturePicker,
} UIModalType;

typedef enum InbePendingSyncAccountAction {
    InbePendingSyncAccountNone = 0,
    InbePendingSyncAccountCreate,
    InbePendingSyncAccountImport
} InbePendingSyncAccountAction;

typedef enum SessionExitModalResult {
    SessionExitModalNone = 0,
    SessionExitModalCancel,
    SessionExitModalSave,
    SessionExitModalDiscard,
} SessionExitModalResult;

typedef enum AppClosePromptResult {
    AppClosePromptNone = 0,
    AppClosePromptKeepRunning,
    AppClosePromptQuit,
} AppClosePromptResult;

typedef struct {
    int active;
    UIModalType type;
} InbeModal;

typedef struct InbeConfig {
	char title[64];
	int width;
	int height;
	int loaded;
    int title_custom;
} InbeConfig;

typedef struct InbeAudioLibraryItem {
    char title[INBE_AUDIO_LABEL_SIZE];
    char path[FS_PATH_MAX];
} InbeAudioLibraryItem;

extern InbeConfig config;

typedef enum ExerciseType {
    EXERCISE_WIM_HOF = 0,
    EXERCISE_MEDITATION = 1,
    EXERCISE_SUN_SALUTATION = 2,
    EXERCISE_COUNT = 3
} ExerciseType;

typedef enum AppThemeMode {
    APP_THEME_SYSTEM = 0,
    APP_THEME_LIGHT = 1,
    APP_THEME_DARK = 2,
} AppThemeMode;

typedef enum AppThemeSource {
    APP_THEME_SOURCE_APP = 0,
    APP_THEME_SOURCE_SYSTEM = 1,
} AppThemeSource;

typedef enum AppOrientationMode {
    APP_ORIENTATION_SYSTEM = 0,
    APP_ORIENTATION_PORTRAIT = 1,
    APP_ORIENTATION_LANDSCAPE = 2,
    APP_ORIENTATION_SENSOR = 3,
} AppOrientationMode;

typedef struct InbeHostApi {
    void *userdata;
    void (*request_size)(void *userdata, int width, int height);
    void (*close)(void *userdata);
} InbeHostApi;

typedef enum NavigationMode {
    NAV_MODE_TABBAR = 0,
    NAV_MODE_DROPDOWN = 1,
} NavigationMode;

typedef enum AppDeviceOrientation {
    APP_DEVICE_ORIENTATION_UNKNOWN = 0,
    APP_DEVICE_ORIENTATION_PORTRAIT = 1,
    APP_DEVICE_ORIENTATION_LANDSCAPE = 2,
} AppDeviceOrientation;

typedef enum AppMainTab {
    APP_MAIN_TAB_NONE = -1,
    APP_MAIN_TAB_HABITS = 0,
    APP_MAIN_TAB_PRACTICE = 1,
} AppMainTab;

typedef enum AppNavRoute {
    APP_NAV_ROUTE_NONE = -1,
    APP_NAV_ROUTE_PROFILE = 0,
    APP_NAV_ROUTE_HABITS = 1,
    APP_NAV_ROUTE_PRACTICE = 2,
    APP_NAV_ROUTE_PET = 3,
    APP_NAV_ROUTE_SETTINGS = 4,
    APP_NAV_ROUTE_STACK = 5,
    APP_NAV_ROUTE_ACCOUNT = 6,
    APP_NAV_ROUTE_DATA = 7,
    APP_NAV_ROUTE_FRIENDS = 8,
    APP_NAV_ROUTE_LEADERBOARD = 9,
    APP_NAV_ROUTE_PRACTICES = 10,
    APP_NAV_ROUTE_COUNT = 11,
} AppNavRoute;

enum {
    APP_BOTTOM_NAV_CONTENT_MAX = 4,
};

typedef struct AppRoute {
    int screen;
    int exercise_type;
    int practice_tab;
    int practice_config_tab;
    int settings_tab;
    int profile_view;
    int profile_tab;
    int habits_screen_mode;
    int habits_tab;
} AppRoute;

int app_draw_close_title_bar(InbeApp *app, const char *title, int height);
int app_draw_close_dropdown_title_bar(InbeApp *app, UITitleBarDropdown dropdown,
                                      int height);

typedef struct WhmPracticeState {
    Texture2D image_1;
    Texture2D image_2;
    Texture2D banner;
} WhmPracticeState;

typedef struct MeditationPracticeState {
    Texture2D image_1;
    Texture2D banner;
    int duration_seconds;
    int remaining_seconds;
    int frame_ticks;
    int complete_waiting;
    int duration_mode;
    int custom_minutes;
    int show_extend_controls;
    int music_track;
    int music_practice_mask;
    int music_practice_tracks[EXERCISE_COUNT];
    int music_loaded;
    int music_playing;
    int music_test_playing;
    int music_fade_out_ticks;
    int music_fade_out_total_ticks;
    int music_archive_extracted;
    int music_network_error_notified;
    Music music;
    RuntimeAssetDownload music_download;
    char music_cache_dir[FS_PATH_MAX];
    char music_status[128];
} MeditationPracticeState;

typedef struct SunSalutationPracticeState {
    Texture2D banner;
    Texture2D pose_sheets[2];
    int step;
    int repetition;
    int repetitions;
    int start_seconds;
    int end_seconds;
    int step_ticks;
    int figure;
} SunSalutationPracticeState;

typedef struct PetPreviewState {
    Texture2D egg;
} PetPreviewState;

typedef struct HabitSessionEditState {
    int scroll;
    int active;
    int kind;
    int round;
    int cursor;
    char path[FS_PATH_MAX];
    char text[16];
} HabitSessionEditState;

typedef struct HabitEditState {
    int active;
    int is_new;
    int index;
    int cursor;
    int description_cursor;
    int description_scroll_y;
    int focused;
    int description_focused;
    char text[INBE_HABIT_NAME_SIZE];
    char description[INBE_HABIT_DESCRIPTION_SIZE];
    Color color;
    int sync_mode;
    int sync_activity;
    int counter_enabled;
} HabitEditState;

struct InbeApp {
    Inbe inbe;
    Inbe settings_preview;
    Inbe start_speed_preview;
    int start_speed_preview_speed;
    Camera2D camera;
    Texture2D icons[UI_ICON_TYPE_COUNT];
    int graphics_reload_requested;
    InbeHostApi host;

    WhmPracticeState whm;
    MeditationPracticeState meditation;
    SunSalutationPracticeState sun_salutation;
    PetPreviewState pet;
    Texture2D easteregg_art;
    Texture2D easteregg_waozi;
    Texture2D font_shapes_texture;
    Sound breath_in_sound;
    Sound breath_out_sound;
    Sound bell_sound;
    int audio_ready;
    int audio_meter_attached;
    float audio_meter_level;
    int sound_volume;
    int music_volume;
    int audio_cue_selected[INBE_AUDIO_CUE_COUNT];
    int audio_custom_sound_count;
    int audio_custom_music_count;
    InbeAudioLibraryItem audio_custom_sounds[INBE_AUDIO_CUSTOM_SOUND_MAX];
    InbeAudioLibraryItem audio_custom_music[INBE_AUDIO_CUSTOM_MUSIC_MAX];
    int retention_marker_enabled;
    int retention_marker_last_bucket;
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
    int profile_view;
    int profile_tab;
    int profile_tab_scroll;
    int profile_scroll;
    int profile_leaderboard_practice;
    int profile_leaderboard_metric;
    int profile_leaderboard_loaded;
    int profile_friend_input_cursor;
    int profile_friend_input_focused;
    int profile_friends_loaded;
    char profile_friend_input[80];
    char profile_pending_friend_remove_id[80];
    char profile_pending_friend_remove_name[96];
    char profile_friends_json[8192];
    char profile_friend_requests_json[8192];
    char profile_leaderboard_json[8192];
    int sync_server_url_cursor;
    int sync_server_url_focused;
    char sync_server_url[256];
    int sync_alias_cursor;
    int sync_alias_focused;
    int sync_alias_then_backup;
    char sync_alias_input[40];
    KsyncAccount pending_sync_account;
    int pending_sync_account_action;
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
    int habits_guide_step;
    int habits_guide_seen;
    int exercise_manual_seen_mask;
    int practice_visible_mask;
    int practice_home_scroll;

    int theme_id;
    int theme_source;
    int dark_mode;
    int theme_mode;
    int theme_style;
    int orientation_mode;
    int ui_scale_tenths;
    int navigation_mode;
    int bottom_nav_routes[APP_BOTTOM_NAV_CONTENT_MAX];
    int bottom_nav_route_count;
    int bottom_nav_config_routes[APP_BOTTOM_NAV_CONTENT_MAX];
    int bottom_nav_config_route_count;
    int nav_sidebar_open;
    int nav_sidebar_open_frame;
    int nav_sidebar_scroll;
    int nav_sidebar_return_on_back;
    int nav_sidebar_prior_screen;
    UIIconType profile_picture_icon;
    int profile_picture_picker_scroll;
    int android_orientation;
    int main_tab;
    InbeHabits habits;
    int habit_detail_index;
    int habit_detail_day;
    char habit_detail_session_path[FS_PATH_MAX];
    HabitSessionEditState habit_session_edit;
    HabitEditState habit_edit;
    int habit_counter_press_day;
    int habit_counter_press_index;
    int habit_counter_press_frames;
    int habit_counter_press_long_done;
    int habit_counter_press_start_x;
    int habit_counter_press_start_y;
    int advanced_session_controls;
    int double_tap_to_breathe;
    double breath_tap_last_time;
    int exercise_type;
    int practice_tab;
    int practice_config_tab;
    int practice_category_tab;
    int practice_coming_soon_ticks;
    int previous_screen;
    int file_dialog_active;
    int session_paused;
    int backgrounded;
    double desktop_background_last_time;
    int results_saved;
    int input_block_frame;
    int close_prompt_open;
    int close_prompt_input_block_frame;
    AppClosePromptResult close_prompt_result;
    int request_quit;   /* desktop no-tray build: app layer requests exit */
    char results_path[FS_PATH_MAX];
    int volume_popup_active;
    InbeModal modal;
    int play_circle_hover;
    float play_circle_scale;
    SettingsThemeState theme_state;
};

void app_set_host_api(InbeApp *app, InbeHostApi host);
void app_init(void *app);
void app_update_draw(void *app, Rectangle viewport);
void app_destroy(void *app);
InbeApp *get_global_inbe_app(void);
void set_global_inbe_app(InbeApp *app);
void app_switch_screen(InbeApp *app, int screen);
AppRoute app_current_route(const InbeApp *app);
void app_switch_route(InbeApp *app, AppRoute route);
void app_leave_practice_config(InbeApp *app);
int app_content_top_reserved(const InbeApp *app);
int app_toolbar_height(void);
int app_auto_sync(InbeApp *app);
void app_request_social_refresh(InbeApp *app);
void app_request_friend_send(InbeApp *app, const char *target);
void app_request_friend_accept(InbeApp *app, const char *request_id);
void app_request_friend_decline(InbeApp *app, const char *request_id);
void app_request_friend_remove(InbeApp *app, const char *friend_user_id);
int app_should_use_tab_bar(const InbeApp *app);
void app_play_breath_cue(InbeApp *app, int dir);
void app_play_bell_cue(InbeApp *app, float scale);
void app_play_sound(InbeApp *app, Sound sound, float scale);
int app_audio_reinitialize(InbeApp *app);
float app_audio_output_level(InbeApp *app);
int app_bell_cue_playing(InbeApp *app);
void app_audio_library_load(InbeApp *app);
void app_audio_library_save(const InbeApp *app);
void app_audio_reload_cue_sounds(InbeApp *app);

/* Audio import error codes */
#define AUDIO_IMPORT_SUCCESS 1
#define AUDIO_IMPORT_ERROR_INVALID_PATH -1
#define AUDIO_IMPORT_ERROR_INVALID_FORMAT -2
#define AUDIO_IMPORT_ERROR_FILE_NOT_FOUND -3
#define AUDIO_IMPORT_ERROR_COPY_FAILED -4
#define AUDIO_IMPORT_ERROR_UNKNOWN -5

int app_audio_import_custom_sound_ex(InbeApp *app, int cue, const char *path, int *error_code);
int app_audio_import_custom_music_ex(InbeApp *app, const char *path, int *error_code);
int app_audio_import_custom_sound(InbeApp *app, int cue, const char *path);
int app_audio_import_custom_music(InbeApp *app, const char *path);
int app_audio_remove_custom_sound(InbeApp *app, int index);
int app_audio_remove_custom_music(InbeApp *app, int index);
int app_audio_music_count(const InbeApp *app);
const char *app_audio_music_label(const InbeApp *app, int index);
int app_audio_music_path(const InbeApp *app, int index, char *out, size_t out_size);
int app_audio_sound_file_valid(const char *path);
int app_audio_music_file_valid(const char *path);
const char *app_audio_cue_default_asset(int cue);
int app_audio_cue_path(InbeApp *app, int cue, char *out, size_t out_size);
void app_audio_music_sanitize_selection(InbeApp *app);
Texture2D app_load_asset_texture(const char *name);
void app_unload_texture(Texture2D texture);

void app_open_modal(InbeApp *app, UIModalType type);
void app_close_modal(InbeApp *app);
void app_block_pointer_frame(InbeApp *app);
void app_request_desktop_close(InbeApp *app);
void app_request_desktop_quit(InbeApp *app);
AppClosePromptResult app_consume_close_prompt_result(InbeApp *app);
SessionExitModalResult app_draw_session_exit_modal(int can_save,
                                                   const char *save_message,
                                                   const char *discard_message);

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

#include "app_nav.h"
#include "app/app_settings.h"

#endif
