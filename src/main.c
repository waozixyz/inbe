#include "raylib.h"
#include "app.h"
#include "android_insets.h"

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

static InbeApp inbe_app;

#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
#define INBE_ANDROID_BUILD 1
#else
#define INBE_ANDROID_BUILD 0
#endif

static AndroidInsets insets;

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

#if INBE_ANDROID_BUILD
    android_insets_get(&insets);

    int safe_top = insets.cutout_top > insets.status_bar ?
                   insets.cutout_top : insets.status_bar;
    int safe_bottom = insets.cutout_bottom > insets.nav_bar ?
                      insets.cutout_bottom : insets.nav_bar;
    int safe_left = insets.cutout_left;
    int safe_right = insets.cutout_right;

    int content_x = safe_left;
    int content_y = safe_top;
    int content_width = width - safe_left - safe_right;
    int content_height = height - safe_top - safe_bottom;

    TraceLog(LOG_INFO, "FRAME: screen=%dx%d safe=%d,%d,%d,%d content=%d,%d,%dx%d",
	     width, height, safe_left, safe_top, safe_right, safe_bottom,
	     content_x, content_y, content_width, content_height);

    BeginDrawing();
    ClearBackground(BLACK);
    inbe_app_update_draw(&inbe_app, (Rectangle){
        (float)content_x,
        (float)content_y,
        (float)content_width,
        (float)content_height
    });
    if(inbe_app.cursor_clickable)
        SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
    else
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
#else
    BeginDrawing();
    ClearBackground(BLACK);
    inbe_app_update_draw(&inbe_app, (Rectangle){
        0,
        0,
        (float)width,
        (float)height
    });
    if(inbe_app.cursor_clickable)
        SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
    else
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
#endif
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
    android_insets_init();
#endif

    inbe_app_init(&inbe_app);

    /* Apply fullscreen setting on startup */
    if(inbe_app.fullscreen_enabled && !IsWindowFullscreen())
        ToggleFullscreen();

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