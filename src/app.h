#ifndef INBE_APP_H
#define INBE_APP_H

#include "raylib.h"
#include "lotus_app.h"
#include "../libinbe/inbe.h"

typedef struct InbeApp InbeApp;

struct InbeApp {
    Inbe inbe;
    Camera2D camera;
    int cursor_clickable;
};

void inbe_app_init(void *app);
void inbe_app_update_draw(void *app, Rectangle viewport);
const char *inbe_app_title(void);
int inbe_app_width(void);
int inbe_app_height(void);
const LotusAppApi *inbe_app_api(void);

#endif
