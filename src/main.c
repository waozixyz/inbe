#include "raylib.h"
#include "app.h"
#include "ui/dpi.h"
#include <stddef.h>
#include <stdio.h>

#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
#include "android_insets.h"
#include "android_wakelock.h"
#include "android_timer.h"
#include <android/log.h>
#include <android_native_app_glue.h>
extern struct android_app *GetAndroidApp(void);
#endif

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

static InbeApp inbe_app;
static InbeApp *g_inbe_app_ptr = NULL;  // Global pointer for JNI access

InbeApp* get_global_inbe_app(void) {
    if (g_inbe_app_ptr == NULL) {
        TraceLog(LOG_WARNING, "WARNING: get_global_inbe_app called with NULL pointer");
    }
    return g_inbe_app_ptr;
}

void set_global_inbe_app(InbeApp *app);

#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
#define INBE_ANDROID_BUILD 1
#else
#define INBE_ANDROID_BUILD 0
#endif

#if INBE_ANDROID_BUILD
static AndroidInsets insets;
#endif

#if !defined(PLATFORM_ANDROID) && !defined(__ANDROID__) && !defined(ANDROID) && !defined(PLATFORM_WEB)
static void
use_packaged_app_directory(void)
{
    char probe_path[512];
    const char *app_dir = GetApplicationDirectory();

    if(app_dir == NULL || app_dir[0] == '\0')
        return;

    snprintf(probe_path, sizeof(probe_path), "%sassets/sounds/bell.ogg", app_dir);
    if(FileExists(probe_path))
        ChangeDirectory(app_dir);
}
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

    BeginDrawing();
    ClearBackground(BLACK);
    BeginScissorMode(content_x, content_y, content_width, content_height);
    inbe_app_update_draw(&inbe_app, (Rectangle){
        (float)content_x,
        (float)content_y,
        (float)content_width,
        (float)content_height
    });
    EndScissorMode();
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
    int window_w = INBE_ANDROID_BUILD ? 0 : config.width;
    int window_h = INBE_ANDROID_BUILD ? 0 : config.height;

#if INBE_ANDROID_BUILD
    __android_log_write(ANDROID_LOG_INFO, "INBE_MAIN", "=== MAIN START ===");
#endif

#if !defined(PLATFORM_ANDROID) && !defined(__ANDROID__) && !defined(ANDROID) && !defined(PLATFORM_WEB)
    use_packaged_app_directory();
#endif

#if !defined(PLATFORM_ANDROID) && !defined(__ANDROID__) && !defined(ANDROID) && !defined(PLATFORM_WEB)
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
#endif

#if INBE_ANDROID_BUILD
    __android_log_write(ANDROID_LOG_INFO, "INBE_MAIN", "Calling android_insets_init");
    android_insets_init();
    __android_log_write(ANDROID_LOG_INFO, "INBE_MAIN", "Calling android_wakelock_init");
    android_wakelock_init();
    __android_log_write(ANDROID_LOG_INFO, "INBE_MAIN", "Calling android_timer_init");
    android_timer_init();
    __android_log_write(ANDROID_LOG_INFO, "INBE_MAIN", "Init calls done");
    if(!ChangeDirectory("/data/user/0/xyz.waozi.inbe/files"))
        TraceLog(LOG_WARNING, "INBE: failed to switch to Android files directory");
#endif


    InitWindow(window_w, window_h, config.title);

    dpi_init();
    inbe_app_init(&inbe_app);
    set_global_inbe_app(&inbe_app);
    TraceLog(LOG_INFO, "INBE: Global app pointer set");

    #if INBE_ANDROID_BUILD
    inbe_app.fullscreen_enabled = 0;
    #endif
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

void set_global_inbe_app(InbeApp *app) {
    g_inbe_app_ptr = app;
    TraceLog(LOG_INFO, "INBE: Global app pointer set to %p", app);
}
