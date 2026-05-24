#ifndef INBE_RAYLIB_H
#define INBE_RAYLIB_H

#include "raylib.h"
#include "inbe_meta.h"
#include "../libinbe/inbe.h"

typedef struct InbeApp InbeApp;

struct InbeApp {
    Inbe inbe;
    Camera2D camera;
    int cursor_clickable;
};

void inbe_raylib_init(void *app);
void inbe_raylib_update_draw(void *app, Rectangle viewport);

#endif
