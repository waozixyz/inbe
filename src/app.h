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
    int settings_scroll;
    int settings_drag_slider;
    int settings_drag_scrollbar;
    int settings_dirty;
};

void inbe_app_init(void *app);
void inbe_app_update_draw(void *app, Rectangle viewport);
const char *inbe_app_title(void);
int inbe_app_width(void);
int inbe_app_height(void);
const LotusAppApi *inbe_app_api(void);

#endif
