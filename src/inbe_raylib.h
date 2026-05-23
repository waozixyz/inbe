#ifndef INBE_RAYLIB_H
#define INBE_RAYLIB_H

#include "raylib.h"
#include "inbe_meta.h"
#include "../libinbe/inbe.h"

typedef struct InbeApp InbeApp;

struct InbeApp {
    Inbe inbe;
    Camera2D camera;
};

void inbe_raylib_init(InbeApp *app);
void inbe_raylib_update_draw(InbeApp *app, Rectangle viewport);

#endif
