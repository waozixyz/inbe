#ifndef INBE_APP_H
#define INBE_APP_H

#include "raylib.h"
#include "lotus_app.h"
#include "../libinbe/inbe.h"

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
    Texture2D telegram_icon;
    Texture2D globe_icon;
    Texture2D stripe_icon;
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
    int settings_drag_content;  /* Drag content area to scroll */
    int settings_drag_content_y;  /* Initial Y position when drag starts */
    int settings_dirty;
    int settings_tab;
    int fullscreen_enabled;  /* Fullscreen mode */
    int manual_scroll;
    int manual_drag_scrollbar;
    int manual_drag_content;
    int manual_drag_content_y;
    int tutorial_step;
    int tutorial_seen;
    int theme_id;
    int dark_mode;
    int history_scroll;
    int history_drag_scrollbar;
    int history_drag_content;
    int history_drag_content_y;
    int history_level;
    int history_year;
    int history_month;
    int history_day;
    char history_record[16];
    int session_paused;
    int results_saved;
    int saved_pause_seconds;
};

void inbe_app_init(void *app);
void inbe_app_update_draw(void *app, Rectangle viewport);
const char *inbe_app_title(void);
int inbe_app_width(void);
int inbe_app_height(void);
const LotusAppApi *inbe_app_api(void);

#endif
