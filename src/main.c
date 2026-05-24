#include "raylib.h"
#include "app.h"

#if defined(PLATFORM_ANDROID)
#include <android/native_app_glue.h>
#endif

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

static InbeApp inbe_app;

static void
frame(void)
{
    BeginDrawing();
    inbe_app_update_draw(&inbe_app, (Rectangle){
        0,
        0,
        (float)GetScreenWidth(),
        (float)GetScreenHeight()
    });
    EndDrawing();
}

#if defined(PLATFORM_ANDROID)
void android_main(struct android_app *app) {
    (void)app;
    InitWindow(0, 0, inbe_app_title());
#else
int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    InitWindow(inbe_app_width(), inbe_app_height(), inbe_app_title());
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
#if !defined(PLATFORM_ANDROID)
    return 0;
#endif
}
