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
    Texture2D angel_image;
    Texture2D begin_image;
    int settings_scroll;
    int settings_drag_slider;
    int settings_drag_scrollbar;
    int settings_dirty;
    int manual_scroll;
    int tutorial_step;
    int tutorial_seen;
    int history_scroll;
    int session_paused;
    int results_saved;
};

void inbe_app_init(void *app);
void inbe_app_update_draw(void *app, Rectangle viewport);
const char *inbe_app_title(void);
int inbe_app_width(void);
int inbe_app_height(void);
const LotusAppApi *inbe_app_api(void);

#endif
