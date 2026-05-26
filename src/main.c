#include "raylib.h"
#include "app.h"

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

static InbeApp inbe_app;

#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
#define INBE_ANDROID_BUILD 1
#else
#define INBE_ANDROID_BUILD 0
#endif

static void
frame(void)
{
    int width = GetScreenWidth();
    int height = GetScreenHeight();
#if !defined(PLATFORM_WEB)
    int render_width = GetRenderWidth();
    int render_height = GetRenderHeight();

    if(render_width > width)
        width = render_width;
    if(render_height > height)
        height = render_height;
#endif

    BeginDrawing();
    ClearBackground(BLACK);
    inbe_app_update_draw(&inbe_app, (Rectangle){
        0,
        0,
        (float)width,
        (float)height
    });
    EndDrawing();
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    int window_w = INBE_ANDROID_BUILD ? 0 : inbe_app_width();
    int window_h = INBE_ANDROID_BUILD ? 0 : inbe_app_height();

#if !defined(PLATFORM_ANDROID) && !defined(__ANDROID__) && !defined(ANDROID) && !defined(PLATFORM_WEB)
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
#endif
    InitWindow(window_w, window_h, inbe_app_title());

#if INBE_ANDROID_BUILD
    if(!ChangeDirectory("/data/user/0/xyz.waozi.inbe/files"))
        TraceLog(LOG_WARNING, "INBE: failed to switch to Android files directory");
#endif

    inbe_app_init(&inbe_app);
    SetTargetFPS(60);

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(frame, 0, 1);
#else
    while(!WindowShouldClose()) {
        frame();
    }

    CloseWindow();
#endif
    return 0;
}