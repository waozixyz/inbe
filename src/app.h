#ifndef INBE_APP_H
#define INBE_APP_H

#include "raylib.h"
#include "../libinbe/inbe.h"

typedef struct FlintTextLayout FlintTextLayout;



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
    SETTINGS_CATEGORY_PRACTICE = 0,
    SETTINGS_CATEGORY_APP,
    SETTINGS_CATEGORY_ABOUT_DATA,
    SETTINGS_CATEGORY_COUNT
};

enum {
    PRACTICE_SUBTAB_BREATHING = 0,
    PRACTICE_SUBTAB_SESSION,
    PRACTICE_SUBTAB_COUNT
};

enum {
    APP_SUBTAB_SOUND = 0,
    APP_SUBTAB_VISUAL,
    APP_SUBTAB_LANGUAGE,
    APP_SUBTAB_COUNT
};

enum {
    ABOUT_DATA_SUBTAB_DATA = 0,
    ABOUT_DATA_SUBTAB_ABOUT,
    ABOUT_DATA_SUBTAB_COUNT
};

enum {
    SETTINGS_TAB_BREATHING = 0,
    SETTINGS_TAB_SESSION,
    SETTINGS_TAB_SOUND,
    SETTINGS_TAB_APPEARANCE,
    SETTINGS_TAB_LANGUAGE,
    SETTINGS_TAB_DATA,
    SETTINGS_TAB_ABOUT,
    SETTINGS_TAB_COUNT
};

typedef enum {
    UIModalNone,
    UIModalConfirmExitSession,
    UIModalConfirmDeleteData,
    UIModalConfirmDeleteHistory,
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

typedef struct LotusAppApi {
    const char *id;
    void *(*create)(void);
    void (*init)(void *state);
    void (*init_args)(void *state, int argc, char **argv);
    void (*update_draw)(void *state, Rectangle viewport);
    void (*destroy)(void *state);
} LotusAppApi;

typedef struct InbeApp InbeApp;

struct InbeApp {
    Inbe inbe;
    Inbe settings_preview;
    Camera2D camera;
    int cursor_clickable;
    Texture2D gear_icon;
    Texture2D x_icon;
    Texture2D manual_icon;
    Texture2D return_icon;
    Texture2D backward_icon;
    Texture2D forward_icon;
    Texture2D play_icon;
    Texture2D pause_icon;
    Texture2D stat_icon;
    Texture2D home_icon;
    Texture2D trash_icon;
    Texture2D pencil_icon;
    Texture2D save_icon;
    Texture2D telegram_icon;
    Texture2D globe_icon;
    Texture2D monero_icon;

    Texture2D angel_image;
    Texture2D begin_image;
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
    int settings_tab;
    int settings_category;
    int settings_sub_tab;
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

    FlintTextLayout *tutorial_layouts[6];
    int tutorial_layouts_initialized;

    int theme_id;
    int dark_mode;
#if defined(LOTUS_BUILD)
    unsigned int lotus_settings_version;
#endif
    int history_scroll;
    int history_drag_scrollbar;
    int history_drag_content;
    int history_drag_content_y;
    int history_level;
    int history_year;
    int history_month;
    int history_day;
    char history_record[16];
    int history_edit_active;
    int history_edit_kind;
    int history_edit_round;
    int history_edit_cursor;
    char history_edit_path[FS_PATH_MAX];
    char history_edit_text[16];
    int history_delete_kind;
    int history_delete_round;
    char history_delete_path[FS_PATH_MAX];
    int advanced_session_controls;
    int session_paused;
    int results_saved;
    char results_path[FS_PATH_MAX];
    int saved_pause_seconds;
    UIModal modal;
};

void inbe_app_init(void *app);
void inbe_app_update_draw(void *app, Rectangle viewport);
void update_session_sounds(InbeApp *app);
const LotusAppApi *inbe_app_api(void);

int clampi(int x, int min, int max);
int int_from_count(const char src[4]);
void count_from_int(char dst[4], int value);
void save_settings(InbeApp *app);
void reset_settings_preview(InbeApp *app);
void update_preview_bounds(Inbe *inbe, int content_w, int max_h);
void apply_settings(Inbe *inbe, int speed, int max_rounds, int max_breaths, int pause_seconds);
void refresh_theme_colors(int theme_id, int dark_mode);
void refresh_locale_dependent_text(InbeApp *app);
void apply_language_selection(InbeApp *app, int language_index, int save_now);
void draw_preview_inbe(Inbe *inbe, int center_x, int center_y);

#endif
